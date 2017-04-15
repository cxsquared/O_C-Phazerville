// Copyright (c) 2016 Patrick Dowling
//
// Author: Patrick Dowling (pld@gurkenkiste.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// Very simple "reference" voltage app

#include "OC_apps.h"
#include "OC_menus.h"
#include "OC_strings.h"
#include "util/util_settings.h"
#include "src/drivers/FreqMeasure/OC_FreqMeasure.h"

enum ReferenceSetting {
  REF_SETTING_OCTAVE,
  REF_SETTING_SEMI,
  REF_SETTING_RANGE,
  REF_SETTING_RATE,
  REF_SETTING_VOLTAGE_SCALING,
  REF_SETTING_LAST
};

class ReferenceChannel : public settings::SettingsBase<ReferenceChannel, REF_SETTING_LAST> {
public:
  void Init(DAC_CHANNEL dac_channel) {
    InitDefaults();

    rate_phase_ = 0;
    mod_offset_ = 0;
    last_pitch_ = 0;
    dac_channel_ = dac_channel;
  
    update_enabled_settings();

  }

  int get_octave() const {
    return values_[REF_SETTING_OCTAVE];
  }

  int32_t get_semitone() const {
    return values_[REF_SETTING_SEMI];
  }

  int get_range() const {
    return values_[REF_SETTING_RANGE];
  }

  uint32_t get_rate() const {
    return values_[REF_SETTING_RATE];
  }

  uint8_t get_voltage_scaling() const {
    return values_[REF_SETTING_VOLTAGE_SCALING];
  }

  void Update() {

    int octave = get_octave();
    int range = get_range();
    if (range) {
      rate_phase_ += OC_CORE_TIMER_RATE;
      if (rate_phase_ >= get_rate() * 1000000UL) {
        rate_phase_ = 0;
        mod_offset_ = 1 - mod_offset_;
      }
      octave += mod_offset_ * range;
    } else {
      rate_phase_ = 0;
      mod_offset_ = 0;
    }

    int32_t semitone = get_semitone();
    OC::DAC::set(dac_channel_, OC::DAC::semitone_to_scaled_voltage_dac(dac_channel_, semitone, octave, get_voltage_scaling()));
    last_pitch_ = (semitone + octave * 12) << 7;
          
  }

  int num_enabled_settings() const {
    return num_enabled_settings_;
  }

  ReferenceSetting enabled_setting_at(int index) const {
    return enabled_settings_[index];
  }

  void update_enabled_settings() {
    ReferenceSetting *settings = enabled_settings_;
    *settings++ = REF_SETTING_OCTAVE;
    *settings++ = REF_SETTING_SEMI;
    *settings++ = REF_SETTING_RANGE;
    *settings++ = REF_SETTING_RATE;
    if (BUCHLA_SUPPORT)
        *settings++ = REF_SETTING_VOLTAGE_SCALING;
     num_enabled_settings_ = settings - enabled_settings_;
  }

  void RenderScreensaver(weegfx::coord_t start_x, uint8_t chan) const;

private:
  uint32_t rate_phase_;
  int mod_offset_;
  int32_t last_pitch_;
  DAC_CHANNEL dac_channel_;

  int num_enabled_settings_;
  ReferenceSetting enabled_settings_[REF_SETTING_LAST];
};

SETTINGS_DECLARE(ReferenceChannel, REF_SETTING_LAST) {
  { 0, -3, 6, "Octave", nullptr, settings::STORAGE_TYPE_I8 },
  { 0, 0, 11, "Semitone", OC::Strings::note_names_unpadded, settings::STORAGE_TYPE_U8 },
  { 0, -3, 3, "Mod range oct", nullptr, settings::STORAGE_TYPE_U8 },
  { 0, 0, 30, "Mod rate (s)", nullptr, settings::STORAGE_TYPE_U8 },
  { 0, 0, 2, "V/octave", OC::voltage_scalings, settings::STORAGE_TYPE_U8 },
};

class ReferencesApp {
public:
  ReferencesApp() { }
  
  void Init() {
    int dac_channel = 0;
    for (auto &channel : channels_)
      channel.Init(static_cast<DAC_CHANNEL>(dac_channel++));

    ui.selected_channel = 0;
    ui.cursor.Init(0, channels_[0].num_enabled_settings() - 1);

    freq_sum_ = 0;
    freq_count_ = 0;
    frequency_ = 0;
    freq_decicents_deviation_ = 0;
    freq_octave_ = 0;
    freq_note_ = 0;
    freq_decicents_residual_ = 0;

  }

  void ISR() {
    for (auto &channel : channels_)
      channel.Update();

      if (FreqMeasure.available()) {
        // average several readings together
        freq_sum_ = freq_sum_ + FreqMeasure.read();
        freq_count_ = freq_count_ + 1;
        if (milliseconds_since_last_freq_ > 1000) {
          frequency_ = FreqMeasure.countToFrequency(freq_sum_ / freq_count_);
          freq_sum_ = 0;
          freq_count_ = 0;
          milliseconds_since_last_freq_ = 0;
          freq_decicents_deviation_ = round(12000.0 * log2f(frequency_ / 16.3515)) + 500;
          freq_octave_ = -2 + ((freq_decicents_deviation_)/ 12000) ;
          freq_note_ = (freq_decicents_deviation_ - ((freq_octave_ + 2) * 12000)) / 1000;
          freq_decicents_residual_ = ((freq_decicents_deviation_ - ((freq_octave_ - 1) * 12000)) % 1000) - 500;
        }
      }
      
  }

  ReferenceChannel &selected_channel() {
    return channels_[ui.selected_channel];
  }

  struct {
    int selected_channel;
    menu::ScreenCursor<menu::kScreenLines> cursor;
  } ui;

  ReferenceChannel channels_[4];

float get_frequency( ) {
  return(frequency_) ;
}

float get_cents_deviation( ) {
  return(static_cast<float>(freq_decicents_deviation_) / 10.0) ;
}

float get_cents_residual( ) {
  return(static_cast<float>(freq_decicents_residual_) / 10.0) ;
}

int8_t get_octave( ) {
  return(freq_octave_) ;
}

int8_t get_note( ) {
  return(freq_note_) ;
}

private:
  double freq_sum_;
  uint32_t freq_count_;
  float frequency_ ;
  elapsedMillis milliseconds_since_last_freq_;
  int32_t freq_decicents_deviation_;
  int8_t freq_octave_ ;
  int8_t freq_note_;
  int32_t freq_decicents_residual_;
};


ReferencesApp references_app;

// App stubs
void REFS_init() {
  references_app.Init();
}

size_t REFS_storageSize() {
  return 0;
}

size_t REFS_save(void *) {
  return 0;
}

size_t REFS_restore(const void *) {
  return 0;
}

void REFS_isr() {
  return references_app.ISR();
}

void REFS_handleAppEvent(OC::AppEvent event) {
  switch (event) {
    case OC::APP_EVENT_RESUME:
      references_app.ui.cursor.set_editing(false);
      FreqMeasure.begin();
      break;
    case OC::APP_EVENT_SUSPEND:
    case OC::APP_EVENT_SCREENSAVER_ON:
    case OC::APP_EVENT_SCREENSAVER_OFF:
      break;
  }
}

void REFS_loop() {
}

void REFS_menu() {
  menu::QuadTitleBar::Draw();
  for (uint_fast8_t i = 0; i < 4; ++i) {
    menu::QuadTitleBar::SetColumn(i);
    graphics.print((char)('A' + i));
  }
  menu::QuadTitleBar::Selected(references_app.ui.selected_channel);

  const auto &channel = references_app.selected_channel();
  menu::SettingsList<menu::kScreenLines, 0, menu::kDefaultValueX> settings_list(references_app.ui.cursor);
  menu::SettingsListItem list_item;

  while (settings_list.available()) {
    const int setting = 
      channel.enabled_setting_at(settings_list.Next(list_item));
    const int value = channel.get_value(setting);
    const settings::value_attr &attr = ReferenceChannel::value_attr(setting);

    switch (setting) {
      default:
        list_item.DrawDefault(value, attr);
      break;
    }
  }
}

void print_voltage(int octave, int fraction) {
  graphics.printf("%01d", octave);
  graphics.movePrintPos(-1, 0); graphics.print('.');
  graphics.movePrintPos(-2, 0); graphics.printf("%03d", fraction);
}

void ReferenceChannel::RenderScreensaver(weegfx::coord_t start_x, uint8_t chan) const {

  // Mostly borrowed from QQ

  weegfx::coord_t x = start_x + 26;
  weegfx::coord_t y = 34 ; // was 60
  // for (int i = 0; i < 5 ; ++i, y -= 4) // was i < 12
    graphics.setPixel(x, y);

  int32_t pitch = last_pitch_ ;
  int32_t unscaled_pitch = last_pitch_ ;

  switch (references_app.channels_[chan].get_voltage_scaling()) {
      case 1: // 1.2V/oct
          pitch = (pitch * 19661) >> 14 ;
          break;
      case 2: // 2V/oct
          pitch = pitch << 1 ;
          break;
      default: // 1V/oct
          break;
    }

  pitch += (OC::DAC::kOctaveZero * 12) << 7;
  unscaled_pitch += (OC::DAC::kOctaveZero * 12) << 7;

  
  CONSTRAIN(pitch, 0, 120 << 7);

  int32_t octave = pitch / (12 << 7);
  int32_t unscaled_octave = unscaled_pitch / (12 << 7);
  pitch -= (octave * 12 << 7);
  unscaled_pitch -= (unscaled_octave * 12 << 7);
  int semitone = pitch >> 7;
  int unscaled_semitone = unscaled_pitch >> 7;

  y = 34 - unscaled_semitone * 2; // was 60, multiplier was 4
  if (unscaled_semitone < 6)
    graphics.setPrintPos(start_x + menu::kIndentDx, y - 7);
  else
    graphics.setPrintPos(start_x + menu::kIndentDx, y);
  graphics.print(OC::Strings::note_names_unpadded[unscaled_semitone]);

  graphics.drawHLine(start_x + 16, y, 8);
  graphics.drawBitmap8(start_x + 28, 34 - unscaled_octave * 2 - 1, OC::kBitmapLoopMarkerW, OC::bitmap_loop_markers_8 + OC::kBitmapLoopMarkerW); // was 60

  // Try and round to 3 digits
  switch (references_app.channels_[chan].get_voltage_scaling()) {
      case 1: // 1.2V/oct
          semitone = (semitone * 10000 + 40) / 100;
          break;
      case 2: // 2V/oct
      default: // 1V/oct
          semitone = (semitone * 10000 + 50) / 120;
          break;
    }
  
  semitone %= 1000;
  octave -= OC::DAC::kOctaveZero;


  // We want [sign]d.ddd = 6 chars in 32px space; with the current font width
  // of 6px that's too tight, so squeeze in the mini minus...
  y = menu::kTextDy;
  graphics.setPrintPos(start_x + menu::kIndentDx, y);
  if (octave >= 0) {
    print_voltage(octave, semitone);
  } else {
    graphics.drawHLine(start_x, y + 3, 2);
    if (semitone)
      print_voltage(-octave - 1, 1000 - semitone);
    else
      print_voltage(-octave, 0);
  }
}

void REFS_screensaver() {
  references_app.channels_[0].RenderScreensaver( 0, 0);
  references_app.channels_[1].RenderScreensaver(32, 1);
  references_app.channels_[2].RenderScreensaver(64, 2);
  references_app.channels_[3].RenderScreensaver(96, 3);
  graphics.setPrintPos(2, 44);
  graphics.printf("TR4 %7.3fHz", references_app.get_frequency()) ;
  graphics.setPrintPos(2, 56);
  if (references_app.get_frequency() >= 16.3515) {
    graphics.printf("%+i %s %+7.1fc", references_app.get_octave(), OC::Strings::note_names[references_app.get_note()], references_app.get_cents_residual()) ;
  } else {
    graphics.print("                    ");
  }
}

void REFS_handleButtonEvent(const UI::Event &event) {
  if (OC::CONTROL_BUTTON_R == event.control)
    references_app.ui.cursor.toggle_editing();
}

void REFS_handleEncoderEvent(const UI::Event &event) {
  if (OC::CONTROL_ENCODER_L == event.control) {
    int selected = references_app.ui.selected_channel + event.value;
    CONSTRAIN(selected, 0, 3);
    references_app.ui.selected_channel = selected;
    references_app.ui.cursor.AdjustEnd(references_app.selected_channel().num_enabled_settings() - 1);
  } else if (OC::CONTROL_ENCODER_R == event.control) {
    if (references_app.ui.cursor.editing()) {
        auto &selected_channel = references_app.selected_channel();
        ReferenceSetting setting = selected_channel.enabled_setting_at(references_app.ui.cursor.cursor_pos());
        selected_channel.change_value(setting, event.value);
        selected_channel.update_enabled_settings();
    } else {
      references_app.ui.cursor.Scroll(event.value);
    }
  }
}
