#include "tlu/TLUController.hh"
#include "tlu/USBTracer.hh"
#include "eudaq/OptionParser.hh"
#include "eudaq/Timer.hh"
#include "eudaq/Utils.hh"
#include "eudaq/Exception.hh"
#include "eudaq/counted_ptr.hh"
#include <unistd.h>
#include <string>
#include <fstream>
#include <sstream>
#include <csignal>

using namespace tlu;
using eudaq::to_string;
using eudaq::hexdec;

static sig_atomic_t g_done = 0;

void ctrlchandler(int) {
  g_done = 1;
}

int main(int /*argc*/, char ** argv) {
  eudaq::OptionParser op("TLU Control Utility", "1.0", "A comand-line tool for controlling the Trigger Logic Unit");
  eudaq::Option<std::string> fname(op, "f", "bitfile", "", "filename",
                                   "The bitfile containing the TLU firmware to be loaded");
  eudaq::Option<int>         trigg(op, "t", "trigger", 0, "msecs",
                                   "The interval in milliseconds for internally generated triggers (0 = off)");
  eudaq::Option<int>         dmask(op, "d", "dutmask", 2, "mask",
                                   "The mask for enabling the DUT connections");
  eudaq::Option<int>         vmask(op, "v", "vetomask", 0, "mask",
                                   "The mask for vetoing external triggers");
  eudaq::Option<int>         amask(op, "a", "andmask", 255, "mask",
                                   "The mask for coincidence of external triggers");
  eudaq::Option<int>         omask(op, "o", "ormask", 0, "mask",
                                   "The mask for ORing of external triggers");
  eudaq::Option<int>         ipsel(op, "i", "dutinputs", 3, "value",
                                   "Selects the DUT inputs 0-3 (1=HDMI, 2=LEMO, 3=RJ45)");
  eudaq::Option<int>         emode(op, "e", "error-handler", 2, "value",
                                   "Error handler (0=abort, >0=number of tries before exception)");
  eudaq::Option<int>         fwver(op, "r", "fwversion", 0, "value",
                                   "Firmware version to load (0=auto)");
  eudaq::Option<int>         wait(op, "w", "wait", 1000, "ms",
                                  "Time to wait between updates in milliseconds");
  eudaq::OptionFlag          nots(op, "n", "notimestamp", "Do not read out timestamp buffer");
  eudaq::Option<std::string> sname(op, "s", "save-file", "", "filename",
                                   "The filename to save trigger numbers and timestamps");
  eudaq::Option<std::string> trace(op, "z", "trace-file", "", "filename",
                                   "The filename to save a trace of all usb accesses,\n"
                                   "prepend - for only errors, or + for all data (including block transfers)");
  try {
    op.Parse(argv);
    std::cout << "Using options:\n"
              << "TLU version = " << fwver.Value() << (fwver.Value() == 0 ? " (auto)" : "") << "\n"
              << "Bit file name = '" << fname.Value() << "'" << (fname.Value() == "" ? " (auto)" : "") << "\n"
              << "Trigger interval = " << trigg.Value()
              << (trigg.Value() > 0 ? " ms (" + to_string(1e3/trigg.Value()) + " Hz)" : std::string()) << "\n"
              << "DUT Mask = " << hexdec(dmask.Value()) << "\n"
              << "Veto Mask = " << hexdec(vmask.Value()) << "\n"
              << "And Mask = " << hexdec(amask.Value()) << "\n"
              << "Or Mask = " << hexdec(omask.Value()) << "\n"
              << "DUT inputs = " << ipsel.Value() << "\n"
              << "Save file = '" << sname.Value() << "'" << (sname.Value() == "" ? " (none)" : "") << "\n"
              << std::endl;
    counted_ptr<std::ofstream> sfile;
    if (sname.Value() != "") {
      sfile = new std::ofstream(sname.Value().c_str());
      if (!sfile->is_open()) EUDAQ_THROW("Unable to open file: " + sname.Value());
    }
    signal(SIGINT, ctrlchandler);
    if (trace.Value() != "") {
      std::string fname = trace.Value();
      if (fname[0] == '-') {
        tlu::setusbtracelevel(1);
        fname = std::string(fname, 1);
      } else if (fname[0] == '+') {
        tlu::setusbtracelevel(3);
        fname = std::string(fname, 1);
      } else {
        tlu::setusbtracelevel(2);
      }
      tlu::setusbtracefile(fname);
    }
    TLUController TLU(emode.Value());
    TLU.SetVersion(fwver.Value());
    TLU.SetFirmware(fname.Value());
    TLU.Configure();
    //TLU.FullReset();
    TLU.SetTriggerInterval(trigg.Value());
    TLU.SetDUTMask(dmask.Value());
    TLU.SetVetoMask(vmask.Value());
    TLU.SetAndMask(amask.Value());
    TLU.SetOrMask(omask.Value());
    TLU.SelectDUT(ipsel.Value());
    std::cout << "TLU Version = " << TLU.GetVersion() << "\n"
              << "TLU Serial number = " << TLU.GetSerialNumber() << "\n"
              << "Firmware file = " << TLU.GetFirmware() << "\n"
              << "Firmware version = " << TLU.GetFirmwareID() << "\n"
              << "Library version = " << TLU.GetLibraryID() << "\n"
              << std::endl;

    eudaq::Timer totaltime, lasttime;
    TLU.Start();
    std::cout << "TLU Started!" << std::endl;

    unsigned long total = 0;
    while (!g_done) {
      TLU.Update(!nots.IsSet());
      std::cout << std::endl;
      TLU.Print(!nots.IsSet());
      if (sfile.get()) {
        for (size_t i = 0; i < TLU.NumEntries(); ++i) {
          *sfile << TLU.GetEntry(i).Eventnum() << "\t" << TLU.GetEntry(i).Timestamp() << std::endl;
        }
      }
      total += TLU.NumEntries();
      double hertz = TLU.NumEntries() / lasttime.Seconds();
      double avghertz = total / totaltime.Seconds();
      lasttime.Restart();
      std::cout << "Time: " << totaltime.Formatted("%s.%3") << " s, "
                << "Freq: " << hertz << " Hz, "
                << "Average: " << avghertz << " Hz" << std::endl;
      if (wait.Value() > 0) {
        eudaq::mSleep(wait.Value());
      }
    }
    std::cout << "Quitting..." << std::endl;
    TLU.Stop();
    TLU.Update();
    //TLU.Print();
  } catch (...) {
    return op.HandleMainException();
  }
  return 0;
}
