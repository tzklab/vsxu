/**
* Project: VSXu: Realtime modular visual programming language, music/audio visualizer.
*
* This file is part of Vovoid VSXu.
*
* @author Jonatan Wallmander, Vovoid Media Technologies AB Copyright (C) 2003-2013
* @see The GNU Public License (GPL)
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
* or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
* for more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#pragma once

#include "vsx_application.h"
#include <vsx_application_control.h>
#include <audiovisual/vsx_state_manager.h>
#include "player_overlay.h"
#include <vsx_application_input_state_manager.h>
#include <audiovisual/vsx_state_fx_save.h>
#include <perf/vsx_perf.h>
#include <string/vsx_json_helper.h>

#include <SDL2/SDL.h>
#include "../../../lib/application/src/sdl/vsx_application_sdl_window_holder.h"

class player_application
    : public vsx_application
{
  vsx_overlay* overlay = 0x0;
  bool no_overlay = false;
  vsx_string<> filename;
  int count = 0;
  std::vector<double> boundaries;
  std::vector<int> presets;
  bool force_state = false;
  bool force_state_first_drew = false;
  int force_state_num = 0;
  bool output = false;
  std::string output_dir;

public:

  size_t window_title_i = 0;
  void update_window_title()
  {
    req(!(window_title_i++ % 60));
    vsx_perf perf;
    char titlestr[ 200 ];
    sprintf( titlestr, "Vovoid VSXu Player %s [%s %d-bit] [%d MB RAM used] %s", VSXU_VER, PLATFORM_NAME, PLATFORM_BITS, perf.memory_currently_used(), VSXU_VERSION_COPYRIGHT);
    window_title = vsx_string<>(titlestr);
    vsx_application_control::get_instance()->window_title = window_title;
  }

  player_application()
  {
    update_window_title();
    vsx_application_control::get_instance()->create_preferences_path_request();
  }

  void print_help()
  {
    vsx_application::print_help();
    vsx_printf(
      L"    -pl                       Preload all visuals on start \n"
       "    -file [file.json]         \n"
       "    -rd                       randomize / ignore json preset     \n"
       "    -pn [num]                 \n"
    );
  }

  void init_graphics()
  {
    no_overlay = vsx_argvector::get_instance()->has_param("no");

    vsx_module_list_manager::get()->module_list = vsx_module_list_factory_create();
    vsx::engine::audiovisual::state_manager::create();

    // create a new manager
    vsx::engine::audiovisual::state_manager::get()->option_preload_all = vsx_argvector::get_instance()->has_param("pl");

    // init manager with the shared path and sound input type.
    vsx::engine::audiovisual::state_manager::get()->load( (PLATFORM_SHARED_FILES).c_str());

    // create a new text overlay
    overlay = new vsx_overlay;

    vsx::engine::audiovisual::state_manager::get()->set_randomizer(false);
    vsx::engine::audiovisual::state_manager::get()->set_sequential(false);
    vsx::engine::audiovisual::state_manager::get()->select_state(0);

    if (force_state = vsx_argvector::get_instance()->has_param_with_value("pn")){
        force_state_num = vsx_string_helper::s2i(vsx_argvector::get_instance()->get_param_value("pn"));
    }

    if (output = vsx_argvector::get_instance()->has_param_with_value("output")){
        output_dir = vsx_argvector::get_instance()->get_param_value("output").c_str();
    }

    if (vsx_argvector::get_instance()->has_param_with_value("file"))
    {
        filename = vsx_argvector::get_instance()->get_param_value("file");
        vsx_printf(L"file: %hs\n", filename.c_str());

        vsx::json json = vsx::json_helper::load_json_from_file(filename, vsx::filesystem::get_instance());
        vsx::json::array bound_jsonarray = json["boundaries"].array_items();

        // load boundaries
        foreach (bound_jsonarray, i) {
            boundaries.push_back(bound_jsonarray[i]["time"].number_value());
        }
        foreach (boundaries, i) {
            vsx_printf(L"%f\n", boundaries[i]);
        }

        // load presets
        if (!vsx_argvector::get_instance()->has_param("rd"))
        {
            foreach (bound_jsonarray, i) {
                presets.push_back(bound_jsonarray[i]["preset"].int_value());
            }
            foreach (presets, i) {
                vsx_printf(L"%d\n", presets[i]);
            }
        }
    }

  }

  bool fx_levels_loaded = false;
  void draw()
  {
    update_window_title();

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    vsx::engine::audiovisual::state_manager::get()->render();

    if (overlay && !no_overlay)
      overlay->render();

    if (!fx_levels_loaded && vsx_application_control::get_instance()->preferences_path.size())
    {
      vsx::engine::audiovisual::fx_load(
          vsx::engine::audiovisual::state_manager::get()->states_get(),
          vsx_application_control::get_instance()->preferences_path + "fx_levels.json"
        );
      fx_levels_loaded = true;
    }

    if (vsx::engine::audiovisual::state_manager::get()->system_message.size())
    {
      vsx_application_control::get_instance()->message_box_title = "Error";
      vsx_application_control::get_instance()->message_box_message = vsx::engine::audiovisual::state_manager::get()->system_message;
    }

    if(!force_state){
        // && ってダメだとそこで評価終えるよね
        if ( ( count < boundaries.size() ) && ( overlay->total_time > boundaries[count] ) ){
            vsx_printf(L"Next Effect\n");
            if (!presets.empty()){
                vsx::engine::audiovisual::state_manager::get()->select_state(presets[count]);
            } else {
                vsx::engine::audiovisual::state_manager::get()->select_random_state();
            }
            count++;
        }
    } else {
        if(!force_state_first_drew){
            vsx::engine::audiovisual::state_manager::get()->select_state(force_state_num);
            force_state_first_drew = true;
        }
    }

    if(output){
        int magic = 1; // 何フレームごと？
        int offset = 300;
        if(overlay->frame_counter > offset && overlay->frame_counter % magic == 0){
            std::string out = output_dir + "/vsxu-" + std::to_string(force_state_num) + "-" + std::to_string(overlay->frame_counter) + ".bmp";
            vsx_printf(L"%hs\n", out.c_str());

            int height = 0, width = 0;
            SDL_GL_GetDrawableSize(vsx_application_sdl_window_holder::get_instance()->window, &width, &height);
            SDL_Surface * image = SDL_CreateRGBSurface(SDL_SWSURFACE, width, height, 24, 0x000000FF, 0x0000FF00, 0x00FF0000, 0);
            glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, image->pixels);
            SDL_SaveBMP(image, out.c_str());
            SDL_FreeSurface(image);
        }
    }

  }

  void event_key_down(long key)
  {
    switch (key)
    {
      case VSX_SCANCODE_ESCAPE:
        vsx_application_control::get_instance()->shutdown_request();
      case VSX_SCANCODE_PAGEUP:
        vsx::engine::audiovisual::state_manager::get()->speed_inc();
        break;
      case VSX_SCANCODE_PAGEDOWN:
        vsx::engine::audiovisual::state_manager::get()->speed_dec();
        break;
      case VSX_SCANCODE_UP:
        vsx::engine::audiovisual::state_manager::get()->fx_level_inc();
        overlay->show_fx_graph();
        break;
      case VSX_SCANCODE_DOWN:
        vsx::engine::audiovisual::state_manager::get()->fx_level_dec();
        overlay->show_fx_graph();
        break;
      case VSX_SCANCODE_LEFT:
        vsx::engine::audiovisual::state_manager::get()->select_prev_state();
        break;
      case VSX_SCANCODE_RIGHT:
        vsx::engine::audiovisual::state_manager::get()->select_next_state();
        break;
      case VSX_SCANCODE_F1:
        overlay->set_help(1);
        break;
      case VSX_SCANCODE_F:
        overlay->set_help(2);
        break;
      case VSX_SCANCODE_R:
        if (vsx_input_keyboard.pressed_ctrl())
          vsx::engine::audiovisual::state_manager::get()->select_random_state();
        else
        {
          vsx::engine::audiovisual::state_manager::get()->toggle_randomizer();
          overlay->show_randomizer_status();
        }
        break;

      case VSX_SCANCODE_RETURN:
        vsx_printf(L"%f\n", overlay->total_time);

        int height = 0, width = 0;
        SDL_GL_GetDrawableSize(vsx_application_sdl_window_holder::get_instance()->window, &width, &height);
        SDL_Surface * image = SDL_CreateRGBSurface(SDL_SWSURFACE, width, height, 24, 0x000000FF, 0x0000FF00, 0x00FF0000, 0);

        glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, image->pixels);

        SDL_SaveBMP(image, "/tmp/sc.bmp");
        SDL_FreeSurface(image);

        // SDL_SaveBMP(SDL_GetWindowSurface(vsx_application_sdl_window_holder::get_instance()->window), "/tmp/sc.bmp");
    }
  }

  void uninit_graphics()
  {
    vsx::engine::audiovisual::fx_save(
        vsx::engine::audiovisual::state_manager::get()->states_get(),
        vsx_application_control::get_instance()->preferences_path + "fx_levels.json"
      );

    vsx::engine::audiovisual::state_manager::destroy();
    vsx_module_list_factory_destroy(vsx_module_list_manager::get()->module_list);
  }

};

