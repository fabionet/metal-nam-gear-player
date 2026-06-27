#include <algorithm>
#include <cmath>
#include <utility>
#include <cassert>

#include "nam_plugin.h"

#define SMOOTH_EPSILON .0001f

#ifndef BYPASS_DB_THRESHOLD
#define BYPASS_DB_THRESHOLD -100
#endif

namespace NAM {
	Plugin::Plugin()
	{
		// prevent allocations on the audio thread
		currentModelPath.reserve(MAX_FILE_NAME + 1);

		bypassThresholdLinear = powf(10, BYPASS_DB_THRESHOLD * 0.05f);

//		NeuralAudio::NeuralModel::SetLSTMLoadMode(
//#ifdef LSTM_PREFER_NAM
//			NeuralAudio::PreferNAMCore
//#else
//			NeuralAudio::PreferRTNeural
//#endif
//		);
//
//		NeuralAudio::NeuralModel::SetWaveNetLoadMode(
//#ifdef WAVENET_PREFER_NAM
//			NeuralAudio::PreferNAMCore
//#else
//			NeuralAudio::PreferRTNeural
//#endif
		//);
	}

	Plugin::~Plugin()
	{
		delete currentModel;
		delete currentModelR;
	}

	bool Plugin::initialize(double sampleRate, const LV2_Feature* const* features) noexcept
	{
		this->sampleRate = sampleRate;

		loader.SetExternalSampleRate((int)sampleRate);

		// for fetching initial options, can be null
		LV2_Options_Option* options = nullptr;

		for (size_t i = 0; features[i]; ++i)
		{
			if (std::string(features[i]->URI) == std::string(LV2_URID__map))
				map = static_cast<LV2_URID_Map*>(features[i]->data);
			else if (std::string(features[i]->URI) == std::string(LV2_WORKER__schedule))
				schedule = static_cast<LV2_Worker_Schedule*>(features[i]->data);
			else if (std::string(features[i]->URI) == std::string(LV2_LOG__log))
				logger.log = static_cast<LV2_Log_Log*>(features[i]->data);
			else if (std::string(features[i]->URI) == std::string(LV2_OPTIONS__options))
				options = static_cast<LV2_Options_Option*>(features[i]->data);
		}
	
		lv2_log_logger_set_map(&logger, map);

		if (!map)
		{
			lv2_log_error(&logger, "Missing required feature: `%s`", LV2_URID__map);

			return false;
		}

		if (!schedule)
		{
			lv2_log_error(&logger, "Missing required feature: `%s`", LV2_WORKER__schedule);

			return false;
		}

		lv2_atom_forge_init(&atom_forge, map);

		uris.atom_Object = map->map(map->handle, LV2_ATOM__Object);
		uris.atom_Float = map->map(map->handle, LV2_ATOM__Float);
		uris.atom_Int = map->map(map->handle, LV2_ATOM__Int);
		uris.atom_Path = map->map(map->handle, LV2_ATOM__Path);
		uris.atom_URID = map->map(map->handle, LV2_ATOM__URID);
		uris.bufSize_maxBlockLength = map->map(map->handle, LV2_BUF_SIZE__maxBlockLength);
		uris.patch_Set = map->map(map->handle, LV2_PATCH__Set);
		uris.patch_Get = map->map(map->handle, LV2_PATCH__Get);
		uris.patch_property = map->map(map->handle, LV2_PATCH__property);
		uris.patch_value = map->map(map->handle, LV2_PATCH__value);
		uris.units_frame = map->map(map->handle, LV2_UNITS__frame);

		uris.model_Path = map->map(map->handle, MODEL_URI);

		if (options != nullptr)
			options_set(this, options);

		eq.prepare(sampleRate, 2);
		depthFilter.prepare(sampleRate, 2);

		return true;
	}

	// runs on non-RT, can block or use [de]allocations
	LV2_Worker_Status Plugin::work(LV2_Handle instance, LV2_Worker_Respond_Function respond, LV2_Worker_Respond_Handle handle,
		uint32_t size, const void* data)
	{
		switch (*(const LV2WorkType*)data)
		{
			case kWorkTypeLoad:
			{
				auto msg = static_cast<const LV2LoadModelMsg*>(data);
				auto nam = static_cast<NAM::Plugin*>(instance);

				NeuralAudio::NeuralModel* model = nullptr;
				NeuralAudio::NeuralModel* model_r = nullptr;
				LV2SwitchModelMsg response = { kWorkTypeSwitch, {}, {}, {} };
				LV2_Worker_Status result = LV2_WORKER_SUCCESS;

				try
				{
					// load model from path
					const size_t pathlen = strlen(msg->path);

					if (pathlen == 0 || pathlen >= MAX_FILE_NAME)
					{
						// avoid logging an error on an empty path.
						// but do clear the model.
						model = nullptr;
					}
					else
					{
						lv2_log_trace(&nam->logger, "Staging model change: `%s`\n", msg->path);

						model = nam->loader.CreateFromFile(msg->path);
						// Load a second independent instance for true dual-mono (right channel)
						model_r = nam->loader.CreateFromFile(msg->path);
					}

					if (model != nullptr)
					{
						response.model = model;
						response.model_r = model_r;

						memcpy(response.path, msg->path, pathlen);
					}
					else if (model_r != nullptr)
					{
						// L failed but R loaded — discard R to keep both nullptr
						delete model_r;
						model_r = nullptr;
					}
				}
				catch (const std::exception&)
				{
				}

				if (model == nullptr)
				{
					response.path[0] = '\0';

					lv2_log_error(&nam->logger, "Unable to load model from: '%s'\n", msg->path);
				}

				respond(handle, sizeof(response), &response);

				return result;
			}

			case kWorkTypeFree:
			{
				auto msg = static_cast<const LV2FreeModelMsg*>(data);
				delete msg->model;
				delete msg->model_r;

				return LV2_WORKER_SUCCESS;
			}

			case kWorkTypeSwitch:
				// should not happen!
				break;
		}

		return LV2_WORKER_ERR_UNKNOWN;
	}

	// runs on RT, right after process(), must not block or [de]allocate memory
	LV2_Worker_Status Plugin::work_response(LV2_Handle instance, uint32_t size,	const void* data)
	{
		if (*(const LV2WorkType*)data != kWorkTypeSwitch)
			return LV2_WORKER_ERR_UNKNOWN;

		auto msg = static_cast<const LV2SwitchModelMsg*>(data);
		auto nam = static_cast<NAM::Plugin*>(instance);

		// prepare reply for deleting old models
		LV2FreeModelMsg reply = { kWorkTypeFree, nam->currentModel, nam->currentModelR };

		// swap current models with new ones
		nam->currentModel = msg->model;
		nam->currentModelR = msg->model_r;
		nam->currentModelPath = msg->path;
		assert(nam->currentModelPath.capacity() >= MAX_FILE_NAME + 1);

		if (nam->currentModel != nullptr)
		{
			int receptiveFieldSize = nam->currentModel->GetReceptiveFieldSize();

			if (receptiveFieldSize > -1)
			{
				// A newly loaded model is prewarmed to have a silent sample history
				nam->silentSamples = receptiveFieldSize;
				nam->smartBypassed = true;
			}
		}

		// send reply
		nam->schedule->schedule_work(nam->schedule->handle, sizeof(reply), &reply);

		// report change to host/ui
		nam->write_current_path();

		return LV2_WORKER_SUCCESS;
	}

	void Plugin::set_max_buffer_size(int size) noexcept
	{
		maxBufferSize = size;

		loader.SetDefaultMaxAudioBufferSize(size);
	}

	void Plugin::process(uint32_t n_samples) noexcept
	{
		lv2_atom_forge_set_buffer(&atom_forge, (uint8_t*)ports.notify, ports.notify->atom.size);
		lv2_atom_forge_sequence_head(&atom_forge, &sequence_frame, uris.units_frame);

		LV2_ATOM_SEQUENCE_FOREACH(ports.control, event)
		{
			if (event->body.type == uris.atom_Object)
			{
				const auto obj = reinterpret_cast<LV2_Atom_Object*>(&event->body);
				if (obj->body.otype == uris.patch_Get)
				{
					write_current_path();
				}
				else if (obj->body.otype == uris.patch_Set)
				{
					const LV2_Atom* property = NULL;
					const LV2_Atom* file_path = NULL;

					lv2_atom_object_get(obj,
					                    uris.patch_property, &property,
					                    uris.patch_value, &file_path,
					                    0);

					if (property && property->type == uris.atom_URID &&
						((const LV2_Atom_URID*)property)->body == uris.model_Path &&
						file_path && file_path->type == uris.atom_Path &&
						file_path->size > 0 && file_path->size < MAX_FILE_NAME)
					{
						LV2LoadModelMsg msg = { kWorkTypeLoad, {} };
						memcpy(msg.path, file_path + 1, file_path->size);
						schedule->schedule_work(schedule->handle, sizeof(msg), &msg);
					}
				}
			}
		}

		if (*(ports.quality_scale) != qualityScale)
		{
			qualityScale = *(ports.quality_scale);

			loader.SetDefaultQualityScaleFactor(qualityScale);

			if (currentModel != nullptr)
			{
				currentModel->SetQualityScaleFactor(qualityScale);
			}
			if (currentModelR != nullptr)
			{
				currentModelR->SetQualityScaleFactor(qualityScale);
			}
		}

		float level;

		float modelInputAdjustmentDB = 0;

		if (currentModel != nullptr)
		{
			modelInputAdjustmentDB = currentModel->GetRecommendedInputDBAdjustment();

#ifdef SMART_BYPASS_ENABLED
			int receptiveFieldSamples = currentModel->GetReceptiveFieldSize();

			if (receptiveFieldSamples > -1)
			{
				for (unsigned int i = 0; i < n_samples; i++)
				{
					if (abs(ports.audio_in_l[i]) <= bypassThresholdLinear)
					{
						silentSamples++;
					}
					else
					{
						silentSamples = 0;
					}
				}

				if (silentSamples >= (uint32_t)receptiveFieldSamples)
				{
					silentSamples = (uint32_t)receptiveFieldSamples;	// Prevent silentSamples growing and eventually overflowing uint32

					if (smartBypassed)
					{
						int bypassMode = (int)*(ports.channel_mode);
						for (unsigned int i = 0; i < n_samples; i++)
						{
							ports.audio_out_l[i] = ports.audio_in_l[i];
							// Mono: mirror L; Dual-Mono / Split-Stereo: pass R input dry
							ports.audio_out_r[i] = (bypassMode == 0)
								? ports.audio_in_l[i]
								: ports.audio_in_r[i];
						}

						return;
					}

					smartBypassed = true; // If we aren't already, we'll be bypassed on the next process call
				}
				else
					smartBypassed = false;
			}
#endif
		}

		// convert input level from db
		float desiredInputLevel = powf(10, (*(ports.input_level) + modelInputAdjustmentDB) * 0.05f);

		if (fabs(desiredInputLevel - inputLevel) > SMOOTH_EPSILON)
		{
			level = inputLevel;
			for (unsigned int i = 0; i < n_samples; i++)
			{
				// do very basic smoothing
				level = (.99f * level) + (.01f * desiredInputLevel);

				ports.audio_out_l[i] = ports.audio_in_l[i] * level;
			}

			inputLevel = level;
		}
		else
		{
			level = inputLevel = desiredInputLevel;

			for (unsigned int i = 0; i < n_samples; i++)
			{
				ports.audio_out_l[i] = ports.audio_in_l[i] * level;
			}
		}

		float modelLoudnessAdjustmentDB = 0;

		if (currentModel != nullptr)
		{
			currentModel->Process(ports.audio_out_l, ports.audio_out_l, n_samples);

			modelLoudnessAdjustmentDB = currentModel->GetRecommendedOutputDBAdjustment();
		}

		// Depth + Resonance (Mesa-style power-amp shaping, post-model, pre-EQ)
		{
			float d = *(ports.depth);
			if (d != depthCached) { depthFilter.setDepth(d); depthCached = d; }
			float r = *(ports.resonance);
			float rf = *(ports.resonance_freq);
			if (r != resCached || rf != resFreqCached)
			{
				depthFilter.setResonance(r, rf);
				resCached = r;
				resFreqCached = rf;
			}
			for (unsigned int i = 0; i < n_samples; i++)
			{
				ports.audio_out_l[i] = depthFilter.processSample(0, ports.audio_out_l[i]);
			}
		}

		// 5-band EQ (post-depth, pre-output-gain)
		{
			float v;
			v = *(ports.eq_bass);
			if (v != eqBassCached) { eq.setBass(v); eqBassCached = v; }
			v = *(ports.eq_presence);
			if (v != eqPresCached) { eq.setPresence(v); eqPresCached = v; }
			v = *(ports.eq_treble);
			if (v != eqTreCached) { eq.setTreble(v); eqTreCached = v; }
			v = *(ports.eq_air);
			if (v != eqAirCached) { eq.setAir(v); eqAirCached = v; }

			float mf = *(ports.eq_mid_freq);
			float mq = *(ports.eq_mid_q);
			float mg = *(ports.eq_mid_gain);
			if (mf != eqMidFreqCached || mq != eqMidQCached || mg != eqMidGainCached)
			{
				eq.setMid(mf, mq, mg);
				eqMidFreqCached = mf;
				eqMidQCached = mq;
				eqMidGainCached = mg;
			}

			for (unsigned int i = 0; i < n_samples; i++)
			{
				ports.audio_out_l[i] = eq.processSample(0, ports.audio_out_l[i]);
			}
		}

		// Convert output level from db
		float desiredOutputLevel = powf(10, (*(ports.output_level) + modelLoudnessAdjustmentDB) * 0.05f);

		if (fabs(desiredOutputLevel - outputLevel) > SMOOTH_EPSILON)
		{
			level = outputLevel;

			for (unsigned int i = 0; i < n_samples; i++)
			{
				// do very basic smoothing
				level = (.99f * level) + (.01f * desiredOutputLevel);

				ports.audio_out_l[i] = ports.audio_out_l[i] * outputLevel;
			}

			outputLevel = level;
		}
		else
		{
			level = outputLevel = desiredOutputLevel;

			for (unsigned int i = 0; i < n_samples; i++)
			{
				ports.audio_out_l[i] = ports.audio_out_l[i] * level;
			}
		}

		// Channel routing for R out — depends on channel_mode
		//   0 = Mono:         R_out mirrors fully-processed L_out
		//   1 = Dual-Mono:    R processed independently through 2nd model instance + per-channel DSP state
		//   2 = Split-Stereo: R_out = R_in dry (unprocessed)
		{
			int mode = (int)*(ports.channel_mode);
			if (mode == 1 && currentModelR != nullptr)
			{
				// Input gain (use settled inputLevel from L pipeline; no separate smoothing)
				for (unsigned int i = 0; i < n_samples; i++)
					ports.audio_out_r[i] = ports.audio_in_r[i] * inputLevel;

				// Model R (independent state)
				currentModelR->Process(ports.audio_out_r, ports.audio_out_r, n_samples);

				// Depth+Resonance — channel 1
				for (unsigned int i = 0; i < n_samples; i++)
					ports.audio_out_r[i] = depthFilter.processSample(1, ports.audio_out_r[i]);

				// 5-band EQ — channel 1
				for (unsigned int i = 0; i < n_samples; i++)
					ports.audio_out_r[i] = eq.processSample(1, ports.audio_out_r[i]);

				// Output gain
				for (unsigned int i = 0; i < n_samples; i++)
					ports.audio_out_r[i] *= outputLevel;
			}
			else if (mode == 2)
			{
				for (unsigned int i = 0; i < n_samples; i++)
					ports.audio_out_r[i] = ports.audio_in_r[i];
			}
			else
			{
				for (unsigned int i = 0; i < n_samples; i++)
					ports.audio_out_r[i] = ports.audio_out_l[i];
			}
		}

		//float dcBlockCoefficient = 1 - (220.0 / sampleRate);

		//for (unsigned int i = 0; i < n_samples; i++)
		//{
		//	float dcInput = ports.audio_out_l[i];

		//	// dc blocker
		//	ports.audio_out_l[i] = ports.audio_out_l[i] - prevDCInput + dcBlockCoefficient * prevDCOutput;

		//	prevDCInput = dcInput;
		//	prevDCOutput = ports.audio_out_l[i];
		//}
	}

	uint32_t Plugin::options_get(LV2_Handle, LV2_Options_Option*)
	{
		// currently unused
		return LV2_OPTIONS_ERR_UNKNOWN;
	}

	uint32_t Plugin::options_set(LV2_Handle instance, const LV2_Options_Option* options)
	{
		auto nam = static_cast<NAM::Plugin*>(instance);

		for (int i=0; options[i].key && options[i].type; ++i)
		{
			if (options[i].key == nam->uris.bufSize_maxBlockLength && options[i].type == nam->uris.atom_Int)
			{
				nam->set_max_buffer_size(*(const int32_t*)options[i].value);
				break;
			}
		}

		return LV2_OPTIONS_SUCCESS;
	}

	LV2_State_Status Plugin::save(LV2_Handle instance, LV2_State_Store_Function store, LV2_State_Handle handle, 
		uint32_t flags, const LV2_Feature* const* features)
	{
		auto nam = static_cast<NAM::Plugin*>(instance);

		lv2_log_trace(&nam->logger, "Saving state\n");

		if (!nam->currentModel)
		{
			return LV2_STATE_SUCCESS;
		}

		LV2_State_Map_Path* map_path = (LV2_State_Map_Path*)lv2_features_data(features, LV2_STATE__mapPath);

		if (map_path == nullptr)
		{
			lv2_log_error(&nam->logger, "LV2_STATE__mapPath unsupported by host\n");

			return LV2_STATE_ERR_NO_FEATURE;
		}

		// Map absolute sample path to an abstract state path
		char* apath = map_path->abstract_path(map_path->handle, nam->currentModelPath.c_str());

		store(handle, nam->uris.model_Path, apath, strlen(apath) + 1, nam->uris.atom_Path,
			LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE);

		LV2_State_Free_Path* free_path = (LV2_State_Free_Path *)lv2_features_data(features, LV2_STATE__freePath);

		if (free_path != nullptr)
		{
			free_path->free_path(free_path->handle, apath);
		}
		else
		{
#ifndef _WIN32	// Can't free host-allocated memory on plugin side under Windows
			free(apath);
#endif
		}

		return LV2_STATE_SUCCESS;
	}

	LV2_State_Status Plugin::restore(LV2_Handle instance, LV2_State_Retrieve_Function retrieve, LV2_State_Handle handle, 
		uint32_t flags, const LV2_Feature* const* features)
	{
		auto nam = static_cast<NAM::Plugin*>(instance);

		// Get model_Path from state
		size_t      size     = 0;
		uint32_t    type     = 0;
		uint32_t    valflags = 0;
		const void* value = retrieve(handle, nam->uris.model_Path, &size, &type, &valflags);

		lv2_log_trace(&nam->logger, "Restoring model '%s'\n", (const char*)value);

		NAM::LV2LoadModelMsg msg = { NAM::kWorkTypeLoad, {} };

		LV2_State_Status result = LV2_STATE_SUCCESS;

		// Check if a path is set
		if (!value || (type != nam->uris.atom_Path))
		{
			msg.path[0] = '\0';
		}
		else
		{
			LV2_State_Map_Path* map_path = (LV2_State_Map_Path*)lv2_features_data(features, LV2_STATE__mapPath);

			if (map_path == nullptr)
			{
				lv2_log_error(&nam->logger, "LV2_STATE__mapPath unsupported by host\n");

				return LV2_STATE_ERR_NO_FEATURE;
			}

			// Map abstract state path to absolute path
			char* path = map_path->absolute_path(map_path->handle, (const char *)value);

			size_t pathLen = strlen(path);

			if (pathLen >= MAX_FILE_NAME)
			{
				lv2_log_error(&nam->logger, "Model path is too long (max %u chars)\n", MAX_FILE_NAME);

				result = LV2_STATE_ERR_UNKNOWN;
			}
			else
			{
				memcpy(msg.path, path, pathLen);
			}

			LV2_State_Free_Path* free_path = (LV2_State_Free_Path*)lv2_features_data(features, LV2_STATE__freePath);

			if (free_path != nullptr)
			{
				free_path->free_path(free_path->handle, path);
			}
			else
			{
#ifndef _WIN32	// Can't free host-allocated memory on plugin side under Windows
				free(path);
#endif
			}
		}

		if (result == LV2_STATE_SUCCESS)
		{
			// Schedule model to be loaded by the provided worker
			nam->schedule->schedule_work(nam->schedule->handle, sizeof(msg), &msg);

			nam->currentModelPath = msg.path;
		}

		return result;
	}

	void Plugin::write_current_path()
	{
		LV2_Atom_Forge_Frame frame;

		lv2_atom_forge_frame_time(&atom_forge, 0);
		lv2_atom_forge_object(&atom_forge, &frame, 0, uris.patch_Set);

		lv2_atom_forge_key(&atom_forge, uris.patch_property);
		lv2_atom_forge_urid(&atom_forge, uris.model_Path);
		lv2_atom_forge_key(&atom_forge, uris.patch_value);
		lv2_atom_forge_path(&atom_forge, currentModelPath.c_str(), (uint32_t)currentModelPath.length() + 1);

		lv2_atom_forge_pop(&atom_forge, &frame);
	}
}
