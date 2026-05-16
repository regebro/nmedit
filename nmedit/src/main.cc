/*
    Nord Modular patch loader
    Copyright (C) 2004 Marcus Andersson

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

#include <stdio.h>
#include <unistd.h>
#include <string>
#include <vector>

#include "nmprotocol/mididriver.h"
#include "nmprotocol/midiexception.h"
#include "nmprotocol/patchmessage.h"
#include "nmprotocol/nmprotocol.h"

#include "pdl/pdlexception.h"

#include "ppf/ppfexception.h"

#include "nmpatch/patch.h"
#include "nmpatch/modulesection.h"
#include "nmpatch/patchexception.h"

#include "nmprotocol/synth.h"
#include "nmprotocol/synthlistener.h"
#include "mainwindow.h"

#include <FL/Fl.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Return_Button.H>
#include <FL/Fl_Window.H>

extern char *optarg;
extern int optind;

class DebugSynthListener : public SynthListener
{
 public:
  
  DebugSynthListener()
  {
    window = new MainWindow();
  }
  
  virtual ~DebugSynthListener() {}

  virtual void newPatchInSlot(int slot, Patch* patch)
  {
    window->newPatch(patch);
  }

  virtual void patchListChanged()
  {
  }

  virtual void slotStateChanged(int slot, bool active,
				bool selected, int voices)
  {
    printf("Slot %d: %d %d %d\n", slot, active, selected, voices);
  }
  
 private:
  
  MainWindow* window; 

};

static string firstExistingPath(const char* paths[], int count)
{
  for (int i = 0; i < count; i++) {
    if (access(paths[i], R_OK) == 0) {
      return string(paths[i]);
    }
  }
  return string(paths[0]);
}

static string displayPortName(const string& midiPort)
{
  size_t sep = midiPort.find('\t');
  if (sep == string::npos) {
    return midiPort;
  }
  return midiPort.substr(0, sep);
}

static string actualPortName(const string& midiPort)
{
  size_t sep = midiPort.find('\t');
  if (sep == string::npos) {
    return midiPort;
  }
  return midiPort.substr(sep + 1);
}

class MidiSetupDialog
{
 public:

  MidiSetupDialog() : accepted(false)
  {
    window = new Fl_Window(430, 170, "Select MIDI Interface");
    window->begin();

    driverChoice = new Fl_Choice(120, 20, 290, 26, "Driver:");
    inputChoice = new Fl_Choice(120, 60, 290, 26, "Input:");
    outputChoice = new Fl_Choice(120, 100, 290, 26, "Output:");

    Fl_Return_Button* okButton =
      new Fl_Return_Button(240, 135, 80, 26, "Connect");
    Fl_Button* cancelButton = new Fl_Button(330, 135, 80, 26, "Cancel");

    okButton->callback(okCB, this);
    cancelButton->callback(cancelCB, this);
    driverChoice->callback(driverChangedCB, this);

    window->end();
    window->set_modal();

    MidiDriver::StringList drivers = MidiDriver::getDrivers();
    for (MidiDriver::StringList::iterator i = drivers.begin();
         i != drivers.end(); i++) {
      driverChoice->add((*i).c_str());
    }

    if (driverChoice->size() > 0) {
      driverChoice->value(0);
      updatePorts();
    }
  }

  ~MidiSetupDialog()
  {
    delete(window);
  }

  bool run(string& driver, string& input, string& output)
  {
    if (driverChoice->size() == 0) {
      return false;
    }

    window->show();
    while (window->shown()) {
      Fl::wait();
    }

    if (!accepted) {
      return false;
    }

    driver = selectedValue(driverChoice);
    input = selectedPort(inputPorts, inputChoice);
    output = selectedPort(outputPorts, outputChoice);

    if (driver.empty() || input.empty() || output.empty()) {
      return false;
    }

    return true;
  }

 private:

  static void driverChangedCB(Fl_Widget*, void* data)
  {
    ((MidiSetupDialog*) data)->updatePorts();
  }

  static void okCB(Fl_Widget*, void* data)
  {
    MidiSetupDialog* dialog = (MidiSetupDialog*) data;
    if (dialog->selectedValue(dialog->driverChoice).empty() ||
        dialog->selectedPort(dialog->inputPorts, dialog->inputChoice).empty() ||
        dialog->selectedPort(dialog->outputPorts, dialog->outputChoice).empty()) {
      return;
    }

    dialog->accepted = true;
    dialog->window->hide();
  }

  static void cancelCB(Fl_Widget*, void* data)
  {
    MidiSetupDialog* dialog = (MidiSetupDialog*) data;
    dialog->accepted = false;
    dialog->window->hide();
  }

  string selectedValue(Fl_Choice* choice)
  {
    const Fl_Menu_Item* selected = choice->mvalue();
    if (selected == 0 || selected->label() == 0) {
      return string();
    }
    return string(selected->label());
  }

  string selectedPort(const vector<string>& ports, Fl_Choice* choice)
  {
    int idx = choice->value();
    if (idx < 0 || idx >= (int) ports.size()) {
      return string();
    }
    return ports[idx];
  }

  void updatePorts()
  {
    inputChoice->clear();
    outputChoice->clear();
    inputPorts.clear();
    outputPorts.clear();

    string driverName = selectedValue(driverChoice);
    if (driverName.empty()) {
      return;
    }

    MidiDriver* tmpDriver = 0;
    try {
      tmpDriver = MidiDriver::createDriver(driverName);

      MidiDriver::StringList inputs = tmpDriver->getMidiInputPorts();
      for (MidiDriver::StringList::iterator i = inputs.begin();
           i != inputs.end(); i++) {
        string actualPort = actualPortName(*i);
        inputPorts.push_back(actualPort);
        inputChoice->add(displayPortName(*i).c_str());
      }

      MidiDriver::StringList outputs = tmpDriver->getMidiOutputPorts();
      for (MidiDriver::StringList::iterator i = outputs.begin();
           i != outputs.end(); i++) {
        string actualPort = actualPortName(*i);
        outputPorts.push_back(actualPort);
        outputChoice->add(displayPortName(*i).c_str());
      }
    }
    catch (MidiException& exception) {
      printf("MidiException: %s (%d)\n",
             exception.getMessage().c_str(),
             exception.getError());
    }

    if (tmpDriver) {
      delete(tmpDriver);
    }

    if (inputChoice->size() > 0) {
      inputChoice->value(0);
    }
    if (outputChoice->size() > 0) {
      outputChoice->value(0);
    }
  }

  Fl_Window* window;
  Fl_Choice* driverChoice;
  Fl_Choice* inputChoice;
  Fl_Choice* outputChoice;
  vector<string> inputPorts;
  vector<string> outputPorts;
  bool accepted;
};

int main(int argc, char** argv)
{
  MidiDriver* driver = 0;

  printf("nmEdit version 1, Copyright (C) 2004 Marcus Andersson\n"
	 "nmEdit comes with ABSOLUTELY NO WARRANTY. This is free "
	 "software,\nand you are welcome to redistribute it under certain "
	 "conditions.\n");

  try {
    const char* midiPDLCandidates[] = {
      "../../libs/codecs/midi.pdl",
      "/usr/local/lib/nmedit-full/midi.pdl",
      "/usr/local/lib/nmprotocol/midi.pdl"
    };
    const char* patchPDLCandidates[] = {
      "../../libs/codecs/patch.pdl",
      "/usr/local/lib/nmedit-full/patch.pdl",
      "/usr/local/lib/nmpatch/patch.pdl"
    };
    const char* modulePPFCandidates[] = {
      "../../libs/libnmpatch/src/module.ppf",
      "/usr/local/lib/nmedit-full/module.ppf",
      "/usr/local/lib/nmpatch/module.ppf"
    };

    MidiMessage::usePDLFile(firstExistingPath(midiPDLCandidates, 3), 0);
    Patch::usePDLFile(firstExistingPath(patchPDLCandidates, 3), 0);
    ModuleSection::usePPFFile(firstExistingPath(modulePPFCandidates, 3));

    string drivername, input, output;
    bool error = false;

    int option;
    while ((option = getopt(argc, argv, "d:i:o:")) != -1) {

      switch(option) {

      case 'd':
	drivername = string(optarg);
	break;

      case 'i':
	input = string(optarg);
	break;
	
      case 'o':
	output = string(optarg);
	break;
	
      case '?':
	error = true;

      default:
	break;
      }
    }

    if (error || optind > argc) {
      printf("usage: %s \\\n"
	     "        -d mididriver \\\n"
	     "        -i midiinput -o midioutput\n"
	     "If no midi settings are provided, a selection dialog is shown.\n\n",
	     argv[0]);

      printf("Available midi drivers:\n");
      MidiDriver::StringList drivers = MidiDriver::getDrivers();
      for (MidiDriver::StringList::iterator i = drivers.begin();
	   i != drivers.end(); i++) {
	printf(" %s\n", (*i).c_str());
	MidiDriver* driver = MidiDriver::createDriver(*i);
	printf("  inputs:");
	MidiDriver::StringList inputs = driver->getMidiInputPorts();
	for (MidiDriver::StringList::iterator j = inputs.begin();
	     j != inputs.end(); j++) {
    printf(" %s", displayPortName(*j).c_str());
	}
	printf("\n  outputs:");
	MidiDriver::StringList outputs = driver->getMidiOutputPorts();
	for (MidiDriver::StringList::iterator j = outputs.begin();
	     j != outputs.end(); j++) {
    printf(" %s", displayPortName(*j).c_str());
	}
	printf("\n\n");
      }
      exit(1);
    }

    if (drivername.empty() || input.empty() || output.empty()) {
      MidiSetupDialog setupDialog;
      if (!setupDialog.run(drivername, input, output)) {
        printf("No MIDI interface selected. Exiting.\n");
        exit(1);
      }
    }

    driver = MidiDriver::createDriver(drivername);
    driver->connect(input, output);
    NMProtocol nmProtocol(driver);
    Synth synth(&nmProtocol);
    DebugSynthListener dsl;
    synth.addListener(&dsl);

    while(1) {
      Fl::check();
      nmProtocol.heartbeat();
      usleep(10000);
    }

  }
  catch (MidiException& exception) {
    printf("MidiException: %s (%d)\n",
	   exception.getMessage().c_str(),
	   exception.getError());
  }
  catch (PDLException& exception) {
    printf("PDLException: %s (%d)\n",
	   exception.getMessage().c_str(),
	   exception.getError());
  }
  catch (PatchException& exception) {
    printf("PatchException: %s (%d)\n",
	   exception.getMessage().c_str(),
	   exception.getError());
  }
  catch (ppf::PPFException& exception) {
    printf("PPFException: %s (%d)\n",
	   exception.getMessage().c_str(),
	   exception.getError());
  }

  if (driver) {
    driver->disconnect();
    delete(driver);
  }
}
