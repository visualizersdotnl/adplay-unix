/*
 * AdPlay/UNIX - OPL2 audio player
 * Copyright (C) 2001 - 2004 Simon Peter <dn.tlp@gmx.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  
 */

#include "defines.h"
#include "alsa.h"

#define DEFAULT_DEVICE	"default"	// Default ALSA output device

ALSAPlayer::ALSAPlayer(Copl *nopl, const char *device, unsigned char bits,
		       int channels, int freq, unsigned long bufsize)
  : EmuPlayer(nopl, bits, channels, freq, bufsize)
{
  snd_pcm_hw_params_t	*hwparams;
  unsigned int		nfreq = freq;
  unsigned long nbufsize;

  const unsigned int samp_size = getsampsize();

  if(!device) device = DEFAULT_DEVICE;

  snd_pcm_hw_params_malloc(&hwparams);

  // Try to open audio device
  if(snd_pcm_open(&pcm_handle, device, SND_PCM_STREAM_PLAYBACK, 0) < 0) {
    message(MSG_ERROR, "error opening PCM device -- %s", device);
    exit(EXIT_FAILURE);
  }

  // Init hwparams with full configuration space
  if(snd_pcm_hw_params_any(pcm_handle, hwparams) < 0) {
    message(MSG_ERROR, "cannot configure this PCM device -- %s", device);
    exit(EXIT_FAILURE);
  }

  // Set access type
  if(snd_pcm_hw_params_set_access(pcm_handle, hwparams,
				  SND_PCM_ACCESS_RW_INTERLEAVED) < 0) {
    message(MSG_ERROR, "error setting access type");
    exit(EXIT_FAILURE);
  }

  // Set sample format
  if (snd_pcm_hw_params_set_format(pcm_handle, hwparams, bits == 16 ?
				   SND_PCM_FORMAT_S16_LE : SND_PCM_FORMAT_S8) < 0) {
    message(MSG_ERROR, "error setting format");
    exit(EXIT_FAILURE);
  }

  // Set sample rate (nearest possible)
  if(snd_pcm_hw_params_set_rate_near(pcm_handle, hwparams, &nfreq, 0) < 0) {
    message(MSG_ERROR, "error setting sample rate");
    exit(EXIT_FAILURE);
  }

  if(nfreq != (unsigned int)freq)
    message(MSG_NOTE, "%d Hz sample rate not supported by your hardware, using "
	    "%d Hz instead", freq, nfreq);

  // Set number of channels
  if(snd_pcm_hw_params_set_channels(pcm_handle, hwparams, channels) < 0) {
    message(MSG_ERROR, "error setting channels");
    exit(EXIT_FAILURE);
  }

  // Set number of periods
  if(snd_pcm_hw_params_set_periods(pcm_handle, hwparams, 4, 0) < 0) {
    message(MSG_ERROR, "error setting periods");
    exit(EXIT_FAILURE);
  }

  // some additional information
  message(MSG_NOTE, "ALSA: sample/frame size %ld (bytes)", samp_size);
  message(MSG_NOTE, "ALSA: buffer size %ld (frames)", bufsize);

  // Set the preferred buffer size (in samples)
  if(snd_pcm_hw_params_set_buffer_size(pcm_handle, hwparams, bufsize) < 0) {
    if (snd_pcm_hw_params_get_buffer_size(hwparams, &nbufsize) < 0) {
      message(MSG_ERROR, "error setting and getting buffer size (returned %ld)", nbufsize);
      exit(EXIT_FAILURE);
  	}
    setbufsize(nbufsize);
    message(MSG_NOTE, "couldn't set buffersize to %ld, using default of %ld instead", bufsize, nbufsize);
  }

  // Apply HW parameter settings to PCM device and prepare device
  if(snd_pcm_hw_params(pcm_handle, hwparams) < 0) {
    message(MSG_ERROR, "error setting HW params");
    exit(EXIT_FAILURE);
  }

  // dev. info (useful!)
  message(MSG_NOTE, "ALSA HW PCM info:");
  snd_output_t *out;
  snd_output_stdio_attach(&out, stderr, 0);
  snd_pcm_dump_hw_setup(pcm_handle, out);

  snd_pcm_hw_params_free(hwparams);
}

ALSAPlayer::~ALSAPlayer()
{
  // stop playback immediately
  snd_pcm_drop(pcm_handle);
  snd_pcm_close(pcm_handle);

  message(MSG_NOTE, "ALSA stream dropped & closed");
}

void ALSAPlayer::output(const void *buf, unsigned long size)
{
  const unsigned int samp_size = getsampsize(); // number of bytes per frame
  const unsigned long buf_size = getbufsize();  // buffer size in frames

  const long frames_to_write = size / samp_size;
  const unsigned long buf_size_frames = buf_size; // FIXME: this is confusing (either refer to frames or bytes)

  // if this happens, we're screwed
  if (frames_to_write > buf_size_frames)
    message(MSG_WARN, "ALSA trying to push more frames (%ld) than buffer size (%ld)", frames_to_write, buf_size_frames);

  snd_pcm_sframes_t written = snd_pcm_writei(pcm_handle, buf, frames_to_write);
  if (written < 0)
    snd_pcm_prepare(pcm_handle);

  if (written != frames_to_write)
  {
    if (written >= 0)
      message(MSG_WARN, "ALSA buffer underrun? to write %ld frames (samp_size %ld) written %ld", frames_to_write, samp_size, written);
    else
      message(MSG_WARN, "ALSA write error: %s", snd_strerror(written));
  }
}
