#define OLC_PGE_APPLICATION

#include "olcPixelGameEngine.h"
#include <fstream>
#include <array>
#include <iostream>
#include <cstdlib>
//#include "old/main_(old).cc"


struct MidiEvent {
  enum class Type {
    NoteOff,
    NoteOn,
    Other
  } event;

  uint8_t nKey = 0;
  uint8_t nVelocity = 0;
  uint32_t nDeltaTick = 0;
};

struct MidiNote {
  uint8_t nKey = 0;
  uint8_t nVelocity = 0;
  uint32_t nStartTime = 0;
  uint32_t nDuration = 0;
};

struct MidiTrack {
  std::string sName;
  std::string sInstrument;
  std::vector <MidiEvent> vecEvents;
  std::vector <MidiNote> vecNotes;
  uint8_t nMaxNote = 64;
  uint8_t nMinNote = 64;
};

class MidiFile {
  public: enum EventName: uint8_t {
    VoiceNoteOff = 0x80,
      VoiceNoteOn = 0x90,
      VoiceAftertouch = 0xA0,
      VoiceControlChange = 0xB0,
      VoiceProgramChange = 0xC0,
      VoiceChannelPressure = 0xD0,
      VoicePitchBend = 0xE0,
      SystemExclusive = 0xF0,
  };

  enum MetaEventName: uint8_t {
    MetaSequence = 0x00,
      MetaText = 0x01,
      MetaCopyright = 0x02,
      MetaTrackName = 0x03,
      MetaInstrumentName = 0x04,
      MetaLyrics = 0x05,
      MetaMarker = 0x06,
      MetaCuePoint = 0x07,
      MetaChannelPrefix = 0x20,
      MetaEndOfTrack = 0x2F,
      MetaSetTempo = 0x51,
      MetaSMPTEOffset = 0x54,
      MetaTimeSignature = 0x58,
      MetaKeySignature = 0x59,
      MetaSequencerSpecific = 0x7F,
  };

  public: MidiFile() {
    
  }

  MidiFile(const std::string & sFileName) {
    ParseFile(sFileName);
  }

  void Clear() {

  }

  bool ParseFile(const std::string & sFileName) {
    std::ifstream ifs;
    ifs.open(sFileName, std::fstream::in | std::ios::binary);
    if(!ifs.is_open())
      return false;

    auto Swap32 = [](uint32_t n) {
      return (((n >> 24) & 0xff) | ((n << 8) & 0xff0000) | ((n >> 8) & 0xff00) | ((n << 24) & 0xff000000));
    };

    auto Swap16 = [](uint16_t n) {
      return ((n >> 8) | (n << 8));
    };

    auto ReadString = [ & ifs](uint32_t nLength) {
      std::string s;
      for(uint32_t i = 0; i < nLength; i++) s += ifs.get();
      return s;
    };

    auto ReadValue = [ & ifs]() {
      uint32_t nValue = 0;
      uint8_t nByte = 0;

      nValue = ifs.get();

      if(nValue & 0x80) {
        nValue &= 0x7F;
        do {
          nByte = ifs.get();
          nValue = (nValue << 7) | (nByte & 0x7F);
        }
        while (nByte & 0x80);
      }
      return nValue;
    };

    uint32_t n32 = 0;
    uint16_t n16 = 0;

    ifs.read((char * ) & n32, sizeof(uint32_t));
    uint32_t nFileID = Swap32(n32);
    ifs.read((char * ) & n32, sizeof(uint32_t));
    uint32_t nHeaderLength = Swap32(n32);
    ifs.read((char * ) & n16, sizeof(uint16_t));
    uint16_t nFormat = Swap16(n16);
    ifs.read((char * ) & n16, sizeof(uint16_t));
    uint16_t nTrackChunks = Swap16(n16);
    ifs.read((char * ) & n16, sizeof(uint16_t));
    uint16_t nDivision = Swap16(n16);

    for(uint16_t nChunk = 0; nChunk < nTrackChunks; nChunk++) {
      std::cout << "===== NEW TRACK" << std::endl;
      ifs.read((char * ) & n32, sizeof(uint32_t));
      uint32_t nTrackID = Swap32(n32);
      ifs.read((char * ) & n32, sizeof(uint32_t));
      uint32_t nTrackLength = Swap32(n32);

      bool bEndOfTrack = false;

      vecTracks.push_back(MidiTrack());

      uint32_t nWallTime = 0;

      uint8_t nPreviousStatus = 0;

      while (!ifs.eof() && !bEndOfTrack) {
        uint32_t nStatusTimeDelta = 0;
        uint8_t nStatus = 0;

        nStatusTimeDelta = ReadValue();

        nStatus = ifs.get();

        if(nStatus < 0x80) {
          nStatus = nPreviousStatus;
          ifs.seekg(-1, std::ios_base::cur);
        }

        if((nStatus & 0xF0) == EventName::VoiceNoteOff) {
          nPreviousStatus = nStatus;
          uint8_t nChannel = nStatus & 0x0F;
          uint8_t nNoteID = ifs.get();
          uint8_t nNoteVelocity = ifs.get();
          vecTracks[nChunk].vecEvents.push_back({
            MidiEvent::Type::NoteOff,
            nNoteID,
            nNoteVelocity,
            nStatusTimeDelta
          });
        } else if((nStatus & 0xF0) == EventName::VoiceNoteOn) {
          nPreviousStatus = nStatus;
          uint8_t nChannel = nStatus & 0x0F;
          uint8_t nNoteID = ifs.get();
          uint8_t nNoteVelocity = ifs.get();
          if(nNoteVelocity == 0)
            vecTracks[nChunk].vecEvents.push_back({
              MidiEvent::Type::NoteOff,
              nNoteID,
              nNoteVelocity,
              nStatusTimeDelta
            });
          else
            vecTracks[nChunk].vecEvents.push_back({
              MidiEvent::Type::NoteOn,
              nNoteID,
              nNoteVelocity,
              nStatusTimeDelta
            });
        } else if((nStatus & 0xF0) == EventName::VoiceAftertouch) {
          nPreviousStatus = nStatus;
          uint8_t nChannel = nStatus & 0x0F;
          uint8_t nNoteID = ifs.get();
          uint8_t nNoteVelocity = ifs.get();
          vecTracks[nChunk].vecEvents.push_back({
            MidiEvent::Type::Other
          });
        } else if((nStatus & 0xF0) == EventName::VoiceControlChange) {
          nPreviousStatus = nStatus;
          uint8_t nChannel = nStatus & 0x0F;
          uint8_t nControlID = ifs.get();
          uint8_t nControlValue = ifs.get();
          vecTracks[nChunk].vecEvents.push_back({
            MidiEvent::Type::Other
          });
        } else if((nStatus & 0xF0) == EventName::VoiceProgramChange) {
          nPreviousStatus = nStatus;
          uint8_t nChannel = nStatus & 0x0F;
          uint8_t nProgramID = ifs.get();
          vecTracks[nChunk].vecEvents.push_back({
            MidiEvent::Type::Other
          });
        } else if((nStatus & 0xF0) == EventName::VoiceChannelPressure) {
          nPreviousStatus = nStatus;
          uint8_t nChannel = nStatus & 0x0F;
          uint8_t nChannelPressure = ifs.get();
          vecTracks[nChunk].vecEvents.push_back({
            MidiEvent::Type::Other
          });
        } else if((nStatus & 0xF0) == EventName::VoicePitchBend) {
          nPreviousStatus = nStatus;
          uint8_t nChannel = nStatus & 0x0F;
          uint8_t nLS7B = ifs.get();
          uint8_t nMS7B = ifs.get();
          vecTracks[nChunk].vecEvents.push_back({
            MidiEvent::Type::Other
          });

        } else if((nStatus & 0xF0) == EventName::SystemExclusive) {
          nPreviousStatus = 0;

          if(nStatus == 0xFF) {
            uint8_t nType = ifs.get();
            uint8_t nLength = ReadValue();

            switch (nType) {
            case MetaSequence:
              std::cout << "Sequence Number: " << ifs.get() << ifs.get() << std::endl;
              break;
            case MetaText:
              std::cout << "Text: " << ReadString(nLength) << std::endl;
              break;
            case MetaCopyright:
              std::cout << "Copyright: " << ReadString(nLength) << std::endl;
              break;
            case MetaTrackName:
              vecTracks[nChunk].sName = ReadString(nLength);
              std::cout << "Track Name: " << vecTracks[nChunk].sName << std::endl;
              break;
            case MetaInstrumentName:
              vecTracks[nChunk].sInstrument = ReadString(nLength);
              std::cout << "Instrument Name: " << vecTracks[nChunk].sInstrument << std::endl;
              break;
            case MetaLyrics:
              std::cout << "Lyrics: " << ReadString(nLength) << std::endl;
              break;
            case MetaMarker:
              std::cout << "Marker: " << ReadString(nLength) << std::endl;
              break;
            case MetaCuePoint:
              std::cout << "Cue: " << ReadString(nLength) << std::endl;
              break;
            case MetaChannelPrefix:
              std::cout << "Prefix: " << ifs.get() << std::endl;
              break;
            case MetaEndOfTrack:
              bEndOfTrack = true;
              break;
            case MetaSetTempo:
              if(m_nTempo == 0) {
                (m_nTempo |= (ifs.get() << 16));
                (m_nTempo |= (ifs.get() << 8));
                (m_nTempo |= (ifs.get() << 0));
                m_nBPM = (60000000 / m_nTempo);
                std::cout << "Tempo: " << m_nTempo << " (" << m_nBPM << "bpm)" << std::endl;
              }
              break;
            case MetaSMPTEOffset:
              std::cout << "SMPTE: H:" << ifs.get() << " M:" << ifs.get() << " S:" << ifs.get() << " FR:" << ifs.get() << " FF:" << ifs.get() << std::endl;
              break;
            case MetaTimeSignature:
              std::cout << "Time Signature: " << ifs.get() << "/" << (2 << ifs.get()) << std::endl;
              std::cout << "ClocksPerTick: " << ifs.get() << std::endl;
              std::cout << "32per24Clocks: " << ifs.get() << std::endl;
              break;
            case MetaKeySignature:
              std::cout << "Key Signature: " << ifs.get() << std::endl;
              std::cout << "Minor Key: " << ifs.get() << std::endl;
              break;
            case MetaSequencerSpecific:
              std::cout << "Sequencer Specific: " << ReadString(nLength) << std::endl;
              break;
            default:
              std::cout << "Unrecognised MetaEvent: " << nType << std::endl;
            }
          }

          if(nStatus == 0xF0) {
            std::cout << "System Exclusive Begin: " << ReadString(ReadValue()) << std::endl;
          }

          if(nStatus == 0xF7) {
            std::cout << "System Exclusive End: " << ReadString(ReadValue()) << std::endl;
          }
        } else {
          std::cout << "Unrecognised Status Byte: " << nStatus << std::endl;
        }
      }
    }
    
    for(auto & track: vecTracks) {
      uint32_t nWallTime = 0;

      std::list<MidiNote> listNotesBeingProcessed;

      for(auto & event: track.vecEvents) {
        
        nWallTime += event.nDeltaTick;

        if(event.event == MidiEvent::Type::NoteOn) {
          listNotesBeingProcessed.push_back({
            event.nKey,
            event.nVelocity,
            nWallTime,
            0
          });
        }
        
        if(event.event == MidiEvent::Type::NoteOff) {
          auto note = std::find_if(listNotesBeingProcessed.begin(), listNotesBeingProcessed.end(), [ & ](const MidiNote & n) {
            return n.nKey == event.nKey;
          });
          if(note != listNotesBeingProcessed.end()) {
            note -> nDuration = nWallTime - note -> nStartTime;
            track.vecNotes.push_back( * note);
            track.nMinNote = std::min(track.nMinNote, note -> nKey);
            track.nMaxNote = std::max(track.nMaxNote, note -> nKey);
            listNotesBeingProcessed.erase(note);
          }
        }
      }
    }
    
    return true;
  }

  public: std::vector<MidiTrack> vecTracks;
  uint32_t m_nTempo = 0;
  uint32_t m_nBPM = 0;

};


class olcMIDIViewer: public olc::PixelGameEngine {
  public: olcMIDIViewer() {
    sAppName = "MIDI File Viewer";
  }

  MidiFile midi;

  HMIDIOUT hInstrument;
  size_t nCurrentNote[16] {
    0
  };

  double dSongTime = 0.0;
  double dRunTime = 0.0;
  uint32_t nMidiClock = 0;



  public: bool OnUserCreate() override {
    unsigned int devCount = midiOutGetNumDevs();
    std::cout << devCount << " MIDI devices connected:" << std::endl;
    MIDIOUTCAPS inputCapabilities;
    for (unsigned int i = 0; i < devCount; i++) {
        midiOutGetDevCaps(i, &inputCapabilities, sizeof(inputCapabilities));
        std::cout << "[" << i << "] " << inputCapabilities.szPname << std::endl;
    }
    midi.ParseFile("audio and or visual/K-Pop.mid");
    int nMidiDevices = midiOutGetNumDevs();
    if(nMidiDevices > 0) {
      if(midiOutOpen(&hInstrument, 1, NULL, 0, NULL) == MMSYSERR_NOERROR) {
        std::cout << "Opened midi" << std::endl;
      }
    }
    
    return true;
  }

  float nTrackOffset = 1000;

  bool OnUserUpdate(float fElapsedTime) override {
    Clear(olc::BLACK);
    uint32_t nTimePerColumn = 50;
    uint32_t nNoteHeight = 2;
    uint32_t nOffsetY = 0;

    if(GetKey(olc::Key::LEFT).bHeld) nTrackOffset -= 10000.0 * fElapsedTime;
    if(GetKey(olc::Key::RIGHT).bHeld) nTrackOffset += 10000.0 * fElapsedTime;

    for(auto & track: midi.vecTracks) {
      if(!track.vecNotes.empty()) {
        uint32_t nNoteRange = track.nMaxNote - track.nMinNote;

        FillRect(0, nOffsetY, ScreenWidth(), (nNoteRange + 1) * nNoteHeight, olc::DARK_GREY);
        DrawString(1, nOffsetY + 1, track.sName);

        for(auto & note: track.vecNotes) {
          FillRect((note.nStartTime - nTrackOffset) / nTimePerColumn, (nNoteRange - (note.nKey - track.nMinNote)) * nNoteHeight + nOffsetY, note.nDuration / nTimePerColumn, nNoteHeight, olc::WHITE);
        }

        nOffsetY += (nNoteRange + 1) * nNoteHeight + 4;
      }
    }
    
    dRunTime += fElapsedTime;
    uint32_t nTempo = 4;
    int nTrack = 1;
    while (dRunTime >= 1.0 / double(midi.m_nBPM * 8)) {
      dRunTime -= 1.0 / double(midi.m_nBPM * 8);
      nMidiClock++;

      int i = 0;
      int nTrack = 1;
      for(nTrack = 1; nTrack < 3; nTrack++) {
        if(nCurrentNote[nTrack] < midi.vecTracks[nTrack].vecEvents.size()) {
          if(midi.vecTracks[nTrack].vecEvents[nCurrentNote[nTrack]].nDeltaTick == 0) {
            uint32_t nStatus = 0;
            uint32_t nNote = midi.vecTracks[nTrack].vecEvents[nCurrentNote[nTrack]].nKey;
            uint32_t nVelocity = midi.vecTracks[nTrack].vecEvents[nCurrentNote[nTrack]].nVelocity;
            if(midi.vecTracks[nTrack].vecEvents[nCurrentNote[nTrack]].event == MidiEvent::Type::NoteOn)
              nStatus = 0x90;
            else
              nStatus = 0x80;

            midiOutShortMsg(hInstrument, (nVelocity << 16) | (nNote << 8) | nStatus);
            nCurrentNote[nTrack]++;
          } else
            midi.vecTracks[nTrack].vecEvents[nCurrentNote[nTrack]].nDeltaTick--;
        }
      }
    }
  
    if(GetKey(olc::Key::SPACE).bPressed) {
      midiOutShortMsg(hInstrument, 0x00403C90);
    }

    if(GetKey(olc::Key::SPACE).bReleased) {
      midiOutShortMsg(hInstrument, 0x00003C80);
    }

    return true;
  }

};

int wmain() {
  //system("g++ old/main_(old).cc -o main_(old)");
  //system("./main_(old).exe");
  olcMIDIViewer demo;
  if(demo.Construct(1200, 800, 1, 1))
    demo.Start();
  return 0;
}
