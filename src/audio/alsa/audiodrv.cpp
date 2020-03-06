/* c-basic-offset: 4; tab-width: 8; indent-tabs-mode: nil
 * vi: set shiftwidth=4 tabstop=8 expandtab:
 * :indentSize=4:tabSize=8:noTabs=true:
 */
// --------------------------------------------------------------------------
// Advanced Linux Sound Architecture (ALSA) specific audio driver interface.
// --------------------------------------------------------------------------
/***************************************************************************
 *  $Log: audiodrv.cpp,v $
 *  Revision 1.7  2002/03/04 19:07:48  s_a_white
 *  Fix C++ use of nothrow.
 *
 *  Revision 1.6  2002/01/10 19:04:00  s_a_white
 *  Interface changes for audio drivers.
 *
 *  Revision 1.5  2001/12/11 19:38:13  s_a_white
 *  More GCC3 Fixes.
 *
 *  Revision 1.4  2001/01/29 01:17:30  jpaana
 *  Use int_least8_t instead of ubyte_sidt which is obsolete now
 *
 *  Revision 1.3  2001/01/23 21:23:23  s_a_white
 *  Replaced SID_HAVE_EXCEPTIONS with HAVE_EXCEPTIONS in new
 *  drivers.
 *
 *  Revision 1.2  2001/01/18 18:35:41  s_a_white
 *  Support for multiple drivers added.  C standard update applied (There
 *  should be no spaces before #)
 *
 *  Revision 1.1  2001/01/08 16:41:43  s_a_white
 *  App and Library Seperation
 *
 ***************************************************************************/

#include "audiodrv.h"
#ifdef   HAVE_ALSA

#include <stdio.h>

Audio_ALSA::Audio_ALSA()
{
    // Reset everything.
    outOfOrder();
}

Audio_ALSA::~Audio_ALSA ()
{
    close ();
}

void
Audio_ALSA::outOfOrder ()
{
    // Reset everything.
    _errorString = "None";
    _audioHandle = NULL;
}

void*
Audio_ALSA::open (AudioConfig &cfg, const char *)
{
    // Transfer input parameters to this object.
    // May later be replaced with driver defaults.
    AudioConfig tmpCfg = cfg;
    int mask, wantedFormat, format;
    int card = -1, dev = 0;

    if (_audioHandle != NULL)
    {
        _errorString = "ERROR: Device already in use";
        return NULL;
    }

    const char *alsa_device = getenv("ALSA_DEFAULT_PCM");
    if (! alsa_device)
      {
	alsa_device = "default";
      }

    int err;
    if ((err = snd_pcm_open(&_audioHandle, alsa_device, SND_PCM_STREAM_PLAYBACK, 0/*SND_PCM_NONBLOCK*/)) < 0)
      {
        _errorString = "ERROR: Could not open audio device: ";
	_errorString += snd_strerror(err);
        goto open_error;
    }

#if 1
    if ((err = snd_spcm_init(_audioHandle,
			     tmpCfg.frequency,
			     tmpCfg.channels,
			     ( tmpCfg.precision == 8 ) ? SND_PCM_FORMAT_U8 : SND_PCM_FORMAT_S16_LE,
			     SND_PCM_SUBFORMAT_STD,
			     SND_SPCM_LATENCY_STANDARD,
			     SND_PCM_ACCESS_RW_INTERLEAVED,
                             SND_SPCM_XRUN_IGNORE)
	 ) < 0) {
      _errorString = "ERROR: Could not set parameters: ";
      _errorString += snd_strerror(err);
      goto open_error;
    }
#else
    if ((err = snd_pcm_set_params(_audioHandle,
                                  ( tmpCfg.precision == 8 ) ? SND_PCM_FORMAT_U8 : SND_PCM_FORMAT_S16_LE,
                                  SND_PCM_ACCESS_RW_INTERLEAVED,
                                  tmpCfg.channels,
                                  tmpCfg.frequency,
                                  1, // soft_resample
                                  500000) // required overall latency in us (0.5s)
	 ) < 0) {
      _errorString = "ERROR: Could not set parameters: ";
      _errorString += snd_strerror(err);
      goto open_error;
    }
#endif

    if (cfg.precision == 16)
      {
	tmpCfg.encoding = AUDIO_SIGNED_PCM;
      }
    else if ( tmpCfg.precision == 8 )
      {
	tmpCfg.encoding = AUDIO_UNSIGNED_PCM;
      }

    {
    snd_pcm_uframes_t buffer_size = 0;
    snd_pcm_uframes_t period_size = 0;
    snd_pcm_get_params(_audioHandle, &buffer_size, &period_size);
    if (_debug)
      {
	fprintf(stderr, "ALSA buffer size: %i, period size: %i\n", (int)buffer_size, (int)period_size);
      }

    // get the current swparams
    snd_pcm_sw_params_t *swparams = NULL;
    snd_pcm_sw_params_alloca(&swparams);
    if ((err = snd_pcm_sw_params_current(_audioHandle, swparams)) < 0)
      {
	_errorString = "ERROR: Could not get sw params: ";
	_errorString += snd_strerror(err);
	goto open_error;
      }
    // start the transfer when the buffer is almost full:
    // (buffer_size / avail_min) * avail_min
    if ((err = snd_pcm_sw_params_set_start_threshold(_audioHandle, swparams, (buffer_size / period_size) * period_size)) < 0)
      {
	_errorString = "ERROR: Unable to set start threshold mode for playback: ";
	_errorString += snd_strerror(err);
	goto open_error;
      }
    // allow the transfer when at least period_size samples can be processed
    // or disable this mechanism when period event is enabled (aka interrupt like style processing)
    if ((err = snd_pcm_sw_params_set_avail_min(_audioHandle, swparams, /*buffer_size*/ period_size)) < 0)
      {
	_errorString = "ERROR: Unable to set avail min for playback: ";
	_errorString += snd_strerror(err);
	goto open_error;
      }
    // write the parameters to the playback device
    if ((err = snd_pcm_sw_params(_audioHandle, swparams)) < 0)
      {
	_errorString = "ERROR: Unable to set sw params for playback: ";
	_errorString += snd_strerror(err);
	goto open_error;
    }
    tmpCfg.bufSize = period_size;
    }

    if (tmpCfg.bufSize < 100)
      {
	_errorString = "ERROR: strange buffer size: ";
	_errorString += std::to_string(tmpCfg.bufSize);
	goto open_error;
      }

    _sampleBuffer = malloc(tmpCfg.bufSize);
    if (!_sampleBuffer)
    {
        _errorString = "AUDIO: Unable to allocate memory for sample buffers.";
        goto open_error;
    }

    // Setup internal Config
    _settings = tmpCfg;
    // Update the users settings
    getConfig (cfg);
    if (_debug)
      {
	fprintf(stderr, "\nset up ALSA %s for %i channels, %i samples/sec, %i bits/sample, buffer size %i bytes\n", alsa_device, (int)tmpCfg.channels, (int)tmpCfg.frequency, (int)tmpCfg.precision, (int)tmpCfg.bufSize);
      }
    return _sampleBuffer;

open_error:
    if (_debug)
      {
	fprintf(stderr, "\n%s\n", _errorString.c_str());
      }
    if (_audioHandle != NULL)
    {
        close ();
    }

    perror ("ALSA");
    return NULL;
}

// Close an opened audio device, free any allocated buffers and
// reset any variables that reflect the current state.
void
Audio_ALSA::close ()
{
    if (_audioHandle != NULL )
    {
        snd_pcm_close(_audioHandle);
	if (_sampleBuffer)
	  {
	    free(_sampleBuffer);
	    _sampleBuffer = NULL;
	  }
        outOfOrder ();
    }
}

void*
Audio_ALSA::reset ()
{
    return _sampleBuffer;
}

void*
Audio_ALSA::write ()
{
    if (_audioHandle == NULL)
    {
        _errorString = "ERROR: Device not open.";
        return NULL;
    }
    snd_pcm_sframes_t frames = snd_pcm_writei(_audioHandle, _sampleBuffer, _settings.bufSize);
    if (frames < 0)
      {
	if (_debug)
	  {
	    fprintf(stderr, "\nwrite failed: %s\n", snd_strerror(frames));
	  }
	frames = snd_pcm_recover(_audioHandle, frames, 0);
      }
    if (frames < 0)
      {
        _errorString = "ERROR: write failed: ";
	_errorString += snd_strerror(frames);
	if (_debug)
	  {
	    fprintf(stderr, "\n%s\n", _errorString.c_str());
	  }
      }
    return _sampleBuffer;
}

void
Audio_ALSA::pause()
{
}

#endif // HAVE_ALSA
