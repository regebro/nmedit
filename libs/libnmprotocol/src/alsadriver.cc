/*
    Nord Modular Midi Protocol 3.03 Library
    Copyright (C) 2003 Marcus Andersson

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifdef ALSA

#include <unistd.h>
#include <glob.h>
#include <stdlib.h>
#include <string.h>
#include <alsa/asoundlib.h>

#include "nmprotocol/midiexception.h"
#include "nmprotocol/alsadriver.h"

static bool parsePortPath(string portPath, int& card, int& device)
{
  return sscanf(portPath.c_str(), "/dev/snd/midiC%dD%d", &card, &device) == 2;
}

static string makeHwPortSpec(int card, int device, int subdevice)
{
  char spec[64];
  snprintf(spec, sizeof(spec), "hw:%d,%d,%d", card, device, subdevice);
  return string(spec);
}

static string makeFallbackLabel(string portPath, int flags)
{
  int card;
  int device;
  if (!parsePortPath(portPath, card, device)) {
    return portPath;
  }

  string label;
  char* cardName = 0;
  if (snd_card_get_name(card, &cardName) == 0 && cardName != 0) {
    label = string(cardName);
    free(cardName);
  }

  char ctlName[32];
  snprintf(ctlName, sizeof(ctlName), "hw:%d", card);

  snd_ctl_t* ctl = 0;
  if (snd_ctl_open(&ctl, ctlName, 0) == 0) {
    snd_rawmidi_info_t* info;
    snd_rawmidi_info_alloca(&info);
    snd_rawmidi_info_set_device(info, device);
    snd_rawmidi_info_set_subdevice(info, 0);
    snd_rawmidi_info_set_stream(
      info,
      (flags & O_RDONLY) ? SND_RAWMIDI_STREAM_INPUT : SND_RAWMIDI_STREAM_OUTPUT
    );

    if (snd_ctl_rawmidi_info(ctl, info) == 0) {
      const char* rawName = snd_rawmidi_info_get_name(info);
      if (rawName != 0 && strlen(rawName) > 0) {
        if (!label.empty()) {
          label += ": ";
        }
        label += string(rawName);
      }
    }

    snd_ctl_close(ctl);
  }

  if (label.empty()) {
    return portPath;
  }

  char portInfo[32];
  snprintf(portInfo, sizeof(portInfo), " (port %d)", device);
  label += portInfo;

  return label;
}

static string makeOpenPortSpec(const string& midiPort)
{
  if (midiPort.find("hw:") == 0) {
    return midiPort;
  }

  int card;
  int device;
  if (parsePortPath(midiPort, card, device)) {
    return makeHwPortSpec(card, device, 0);
  }

  return midiPort;
}

static string decodePortSpec(const string& midiPort)
{
  size_t sep = midiPort.find('\t');
  if (sep == string::npos) {
    return midiPort;
  }
  return midiPort.substr(sep + 1);
}

ALSADriver::ALSADriver()
{
  midi_in = 0;
  midi_out = 0;
}

ALSADriver::~ALSADriver()
{
  disconnect();
}

ALSADriver::StringList ALSADriver::getMidiInputPorts()
{
  return getMidiPorts(O_RDONLY | O_NONBLOCK);
}

ALSADriver::StringList ALSADriver::getMidiOutputPorts()
{
  return getMidiPorts(O_WRONLY | O_NONBLOCK);
}

ALSADriver::StringList ALSADriver::getMidiPorts(int flags)
{
  StringList ports;
  glob_t globdata;
  int fd;

  glob("/dev/snd/midi*", 0, 0, &globdata);
  if (globdata.gl_pathc > 0) {
    int n = 0;
    char* path;
    while ((path = globdata.gl_pathv[n]) != 0) {
      fd = open(path, flags);
      if (fd >= 0) {
  string midiPath(path);

  int card;
  int device;
  if (parsePortPath(midiPath, card, device)) {
    bool addedDevicePorts = false;
    char* cardName = 0;
    string cardLabel;
    if (snd_card_get_name(card, &cardName) == 0 && cardName != 0) {
      cardLabel = string(cardName);
      free(cardName);
    }

    char ctlName[32];
    snprintf(ctlName, sizeof(ctlName), "hw:%d", card);

    snd_ctl_t* ctl = 0;
    if (snd_ctl_open(&ctl, ctlName, 0) == 0) {
      snd_rawmidi_info_t* info;
      snd_rawmidi_info_alloca(&info);
      snd_rawmidi_info_set_device(info, device);
      snd_rawmidi_info_set_subdevice(info, 0);
      snd_rawmidi_info_set_stream(
        info,
        (flags & O_RDONLY) ? SND_RAWMIDI_STREAM_INPUT : SND_RAWMIDI_STREAM_OUTPUT
      );

      if (snd_ctl_rawmidi_info(ctl, info) == 0) {
        unsigned int subdevices = snd_rawmidi_info_get_subdevices_count(info);
        if (subdevices == 0) {
          subdevices = 1;
        }

        for (unsigned int sub = 0; sub < subdevices; sub++) {
          snd_rawmidi_info_set_subdevice(info, sub);
          if (snd_ctl_rawmidi_info(ctl, info) != 0) {
            continue;
          }

          string label = cardLabel;
          const char* subName = snd_rawmidi_info_get_subdevice_name(info);
          const char* rawName = snd_rawmidi_info_get_name(info);
          const char* portName = (subName != 0 && strlen(subName) > 0)
            ? subName
            : rawName;

          if (portName != 0 && strlen(portName) > 0) {
            if (!label.empty()) {
              label += ": ";
            }
            label += string(portName);
          }

          if (label.empty()) {
            label = midiPath;
          }

          char portInfo[32];
          snprintf(portInfo, sizeof(portInfo), " (port %d.%u)", device, sub);
          label += portInfo;

          ports.push_back(label + "\t" + makeHwPortSpec(card, device, sub));
          addedDevicePorts = true;
        }
      }

      snd_ctl_close(ctl);
    }

    if (!addedDevicePorts) {
      string label = makeFallbackLabel(midiPath, flags);
      ports.push_back(label + "\t" + midiPath);
    }
  }
  else {
    ports.push_back(midiPath);
  }

	close(fd);
      }
      n++;
    }
  }
  globfree(&globdata);

  return ports;
}

void ALSADriver::connect(string midiInputPort, string midiOutputPort)
{
  string midiInputPath = makeOpenPortSpec(decodePortSpec(midiInputPort));
  string midiOutputPath = makeOpenPortSpec(decodePortSpec(midiOutputPort));

  int err = snd_rawmidi_open(&midi_in, 0, midiInputPath.c_str(), SND_RAWMIDI_NONBLOCK);
  if (err < 0) {
    throw MidiException("Failed to open midi input port.", -err);
  }

  err = snd_rawmidi_open(0, &midi_out, midiOutputPath.c_str(), SND_RAWMIDI_NONBLOCK);
  if (err < 0) {
    snd_rawmidi_close(midi_in);
    midi_in = 0;
    throw MidiException("Failed to open midi output port.", -err);
  }
}

void ALSADriver::disconnect()
{
  if (midi_in != 0) {
    snd_rawmidi_close(midi_in);
    midi_in = 0;
  }

  if (midi_out != 0) {
    snd_rawmidi_close(midi_out);
    midi_out = 0;
  }
}

void ALSADriver::send(Bytes bytes)
{
  unsigned char buffer[bytes.size()];
  int n;
  Bytes::iterator i;

  for(n = 0, i = bytes.begin(); i != bytes.end(); i++, n++) {
    buffer[n] = (*i);
  }

  int nwrite = snd_rawmidi_write(midi_out, buffer, bytes.size());
  if (nwrite < 0) {
    throw MidiException("Failed to write to midi output port.", -nwrite);
  }
}

void ALSADriver::receive(Bytes& bytes)
{
  unsigned char byte;
  int n;

  bytes.clear();

  while ((n = snd_rawmidi_read(midi_in, &byte, 1)) == 1) {
    inputBuffer.push_back(byte);
    if (byte == SYSEX_END) {
      bytes = inputBuffer;
      inputBuffer.clear();
      return;
    }
  }
  
  if (n < 0 && n != -EAGAIN) {
    throw MidiException("Failed to read from midi input port.", -n);
  }
}

#endif // ALSA
