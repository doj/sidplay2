/* c-basic-offset: 4; tab-width: 8; indent-tabs-mode: nil
 * vi: set shiftwidth=4 tabstop=8 expandtab:
 * :indentSize=4:tabSize=8:noTabs=true:
 */
// --------------------------------------------------------------------------
// Advanced Linux Sound Architecture (ALSA) specific audio driver interface.
// --------------------------------------------------------------------------

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
  AudioConfig tmpCfg;
  snd_pcm_uframes_t buffer_frames;
  snd_pcm_hw_params_t *hw_params;
  int err;

  // Transfer input parameters to this object.
  // May later be replaced with driver defaults.
  tmpCfg = cfg;
  unsigned int rate = tmpCfg.frequency;

  if (_audioHandle != NULL) {
    _errorString = "ERROR: Device already in use";
    return NULL;
  }

  if ((err = snd_pcm_open (&_audioHandle, "default", SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
    _errorString = "ERROR: Could not open audio device: ";
    _errorString += snd_strerror(err);
    goto open_error;
  }

  if ((err = snd_pcm_hw_params_malloc(&hw_params)) < 0) {
    _errorString = "ERROR: could not malloc hwparams: ";
    _errorString += snd_strerror(err);
    goto open_error;
  }

  if ((err = snd_pcm_hw_params_any (_audioHandle, hw_params)) < 0) {
    _errorString = "ERROR: could not initialize hw params: ";
    _errorString += snd_strerror(err);
    goto open_error;
  }

  if (( err = snd_pcm_hw_params_set_access (_audioHandle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
    _errorString = "ERROR: could not set access type: ";
    _errorString += snd_strerror(err);
    goto open_error;
  }

  snd_pcm_format_t alsamode;
  switch (tmpCfg.precision) {
  case 8:
    tmpCfg.encoding = AUDIO_UNSIGNED_PCM;
    alsamode = SND_PCM_FORMAT_U8;
    break;
  case 16:
    tmpCfg.encoding = AUDIO_SIGNED_PCM;
    alsamode = SND_PCM_FORMAT_S16;
    break;
  default:
    _errorString = "ERROR: set desired number of bits for audio device.";
    goto open_error;
  }

  if ((err = snd_pcm_hw_params_set_format (_audioHandle, hw_params, alsamode)) < 0) {
    _errorString = "ERROR: could not set sample format: ";
    _errorString += snd_strerror(err);
    goto open_error;
  }

  if ((err = snd_pcm_hw_params_set_channels (_audioHandle, hw_params, tmpCfg.channels)) < 0) {
    _errorString = "ERROR: could not set channel count: ";
    _errorString += snd_strerror(err);
    goto open_error;
  }

  if ((err = snd_pcm_hw_params_set_rate_near (_audioHandle, hw_params, &rate, 0)) < 0) {
    _errorString = "ERROR: could not set sample rate: ";
    _errorString += snd_strerror(err);
    goto open_error;
  }

  _alsa_to_frames_divisor = tmpCfg.channels * tmpCfg.precision / 8;
  buffer_frames = 4096;
  snd_pcm_hw_params_set_period_size_near(_audioHandle, hw_params, &buffer_frames, 0);

  if ((err = snd_pcm_hw_params (_audioHandle, hw_params)) < 0) {
    _errorString = "ERROR: could not set hw parameters: ";
    _errorString += snd_strerror(err);
    goto open_error;
  }

  snd_pcm_hw_params_free (hw_params);

  if ((err = snd_pcm_prepare (_audioHandle)) < 0) {
    _errorString = "ERROR: could not prepare audio interface for use: ";
    _errorString += snd_strerror(err);
    goto open_error;
  }

  tmpCfg.bufSize = buffer_frames * _alsa_to_frames_divisor;
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
  return _sampleBuffer;

 open_error:
  if (_audioHandle != NULL)
    close ();
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

  snd_pcm_writei (_audioHandle, _sampleBuffer, _settings.bufSize / _alsa_to_frames_divisor);
  return (void *) _sampleBuffer;
}

void
Audio_ALSA::pause()
{
}

#endif // HAVE_ALSA
