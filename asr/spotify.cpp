//
//  spotify.cpp
//  mac
//
//  Created by Paul Ciarlo on 8/23/14.
//
// ASR - Digital Signal Processor
// Copyright (C) 2002-2015	Paul Ciarlo <paul.ciarlo@gmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.	 If not, see <http://www.gnu.org/licenses/>.

#include "config.h"

#if SPOTIFY_ENABLED

#include "spotify.h"

#include <time.h>

#ifdef __MACH__
#include <mach/clock.h>
#include <mach/mach.h>
#endif

const uint8_t g_appkey[] = {
	0x01, 0x5C, 0xE9, 0x00, 0xF6, 0x5E, 0x3D, 0x92, 0xEC, 0x87, 0x19, 0xE3, 0x71, 0xA2, 0x6C, 0x7C,
	0x18, 0x4C, 0xAA, 0xDB, 0x33, 0x6A, 0x4A, 0x27, 0x4B, 0x81, 0xA6, 0xC7, 0xD2, 0x93, 0xDD, 0x4D,
	0xBA, 0xEC, 0x27, 0xF5, 0x44, 0xB2, 0xD5, 0x63, 0xF5, 0x7E, 0xCA, 0xFE, 0x40, 0x11, 0xFC, 0x41,
	0xDB, 0x89, 0xB5, 0xC2, 0x58, 0x42, 0x77, 0xFB, 0x2A, 0xF0, 0x41, 0x19, 0xD5, 0xEB, 0x24, 0x2D,
	0x26, 0x64, 0x00, 0xD3, 0x02, 0x46, 0xDF, 0x07, 0x09, 0x4D, 0x22, 0xA3, 0x00, 0x87, 0x27, 0xFE,
	0xD0, 0xB0, 0x2D, 0x8F, 0x45, 0x28, 0x49, 0x40, 0x03, 0xC9, 0xF8, 0x06, 0x3D, 0x3C, 0x46, 0x6B,
	0x4F, 0xF9, 0x2B, 0xB3, 0xF8, 0xC2, 0xF8, 0x26, 0x11, 0x05, 0xD9, 0x28, 0x5C, 0x4B, 0xD9, 0x4B,
	0xFB, 0x80, 0x9E, 0xF1, 0xC6, 0x15, 0x34, 0xC5, 0x74, 0xD3, 0xCF, 0xD0, 0x38, 0x0B, 0x45, 0x21,
	0x77, 0x96, 0x47, 0x27, 0xE1, 0xD7, 0xA8, 0x0D, 0xDE, 0x93, 0x30, 0x9E, 0xB2, 0x75, 0xBB, 0x24,
	0x7A, 0xD6, 0x30, 0xCC, 0xF3, 0xCF, 0xE7, 0xF3, 0x64, 0xB7, 0xD1, 0xED, 0x5D, 0x1D, 0xCE, 0x16,
	0xD6, 0xE6, 0xBA, 0x1C, 0x9C, 0xA7, 0xEA, 0x24, 0x56, 0x59, 0xEB, 0x82, 0x73, 0x39, 0x4D, 0xCE,
	0x6C, 0x8B, 0xBF, 0x52, 0xE8, 0xBD, 0x4D, 0xF2, 0xA4, 0x63, 0x66, 0xB5, 0x08, 0x46, 0x43, 0x7B,
	0x0E, 0x4C, 0x73, 0x10, 0x31, 0x9F, 0x79, 0x8C, 0x6C, 0x9D, 0xEC, 0x8F, 0x9C, 0x71, 0x90, 0x7E,
	0xD6, 0x99, 0x0D, 0xFD, 0x25, 0x4A, 0x27, 0x93, 0xE6, 0xDB, 0x2A, 0x0D, 0x8E, 0xCE, 0x26, 0x0A,
	0x42, 0x63, 0x45, 0xCA, 0x3D, 0x9A, 0x0A, 0xD4, 0x3C, 0xDE, 0xC7, 0x43, 0x8E, 0x9D, 0x91, 0x90,
	0xDB, 0x21, 0x39, 0x57, 0x0F, 0x47, 0x68, 0xB2, 0xCC, 0xB4, 0x10, 0x63, 0x56, 0xA3, 0xFB, 0xD3,
	0x69, 0x40, 0x7B, 0xF2, 0x69, 0x30, 0x03, 0x28, 0xFC, 0x14, 0x17, 0xF0, 0x12, 0x4D, 0x5B, 0x3D,
	0x41, 0xF0, 0x20, 0x8A, 0xFB, 0xA6, 0x9E, 0xDF, 0xA2, 0x37, 0x7B, 0x87, 0xDA, 0x5A, 0x75, 0x59,
	0x2F, 0xF1, 0x8C, 0x12, 0x4A, 0x6F, 0xB9, 0x94, 0x33, 0x7C, 0x9A, 0x53, 0x31, 0x5C, 0xDD, 0x3B,
	0xB8, 0x87, 0x96, 0xC3, 0xFB, 0x8F, 0xCA, 0x99, 0xC0, 0xAD, 0x0D, 0xD3, 0xEB, 0x14, 0x8C, 0xC2,
	0xF5,
};
const size_t g_appkey_size = sizeof(g_appkey);

Spotify::Spotify() :
_session(0),
_next_timeout(0),
_notify_do(0),
_running(false),
_playlist(0),
_current_track(0),
_want_play(false),
_recipient(0)
{
	pthread_mutex_init(&_lock, NULL);
    pthread_cond_init(&_cond, NULL);
}

Spotify::~Spotify()
{
	logout();
	pthread_cond_destroy(&_cond);
	pthread_mutex_destroy(&_lock);
}

void Spotify::login(const char *username, const char *password)
{
	sp_error err = sp_session_create(&_spconfig, &_session);
	sp_session_preferred_bitrate(_session, SP_BITRATE_320k);
	sp_session_preferred_offline_bitrate(_session, SP_BITRATE_320k, true);
	CHECK_RESULT(err);
	err = sp_session_login(_session, username, password, true, NULL);
	CHECK_RESULT(err);
	_running = true;
	pthread_create(&_thread, NULL, _run_thread, this);
}

void Spotify::logout()
{
	if (_session)
	{
		void *res;
		quit();
		pthread_join(_thread, &res);
		pthread_mutex_lock(&_lock);
		sp_session_logout(_session);
		pthread_cond_wait(&_cond, &_lock);
		pthread_mutex_unlock(&_lock);
		sp_session_release(_session);
		_session = 0;
	}
}

void Spotify::wait_for_playback()
{
	void *ret;
	pthread_join(_thread, &ret);
}

void Spotify::set_recipient(SpotifyMusicRecipient *recipient)
{
	_recipient = recipient;
}

void Spotify::quit()
{
	if (_running)
	{
		_running = false;
		pthread_cond_signal(&_cond);
	}
}

void* Spotify::_run_thread(void *arg)
{
	static_cast<Spotify*>(arg)->run();
	return 0;
}

void Spotify::run()
{
	pthread_mutex_lock(&_lock);
	
    while (_running)
	{
        if (_next_timeout == 0)
		{
            while(!_notify_do && _running)
			{
                pthread_cond_wait(&_cond, &_lock);
			}
        }
		else
		{
            struct timespec ts;
#ifdef __MACH__ // OS X does not have clock_gettime, use clock_get_time
			clock_serv_t cclock;
			mach_timespec_t mts;
			host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
			clock_get_time(cclock, &mts);
			mach_port_deallocate(mach_task_self(), cclock);
			ts.tv_sec = mts.tv_sec;
			ts.tv_nsec = mts.tv_nsec;
			
#else
			clock_gettime(CLOCK_REALTIME, &ts);
#endif
			
            ts.tv_sec += _next_timeout / 1000;
            ts.tv_nsec += (_next_timeout % 1000) * 1000000;
			
            pthread_cond_timedwait(&_cond, &_lock, &ts);
        }
		
        _notify_do = 0;
        pthread_mutex_unlock(&_lock);
		
        //if (g_playback_done) {
		//   track_ended();
		//   g_playback_done = 0;
		// }
		
        do {
            sp_session_process_events(_session, &_next_timeout);
        } while (_next_timeout == 0);
		
        pthread_mutex_lock(&_lock);
    }
	
	pthread_mutex_unlock(&_lock);
}

void Spotify::play_track(SpotifyTrack *track)
{
	_current_track = track;
	set_playlist(track->_pl->get_sp_playlist());
	_want_play = true;
	try_start_playback();
}

void Spotify::try_start_playback()
{
	if (!_current_track || !_playlist || !_want_play) return;
	
	sp_error err = sp_track_error(_current_track->_track);
	if (err != SP_ERROR_OK)
	{
		_current_track = 0;
		return;
	}
	
    err = sp_session_player_load(_session, _current_track->_track);
	if (err != SP_ERROR_OK)
	{
		_current_track = 0;
		return;
	}
    err = sp_session_player_play(_session, true);
	if (err != SP_ERROR_OK)
	{
		_current_track = 0;
		return;
	}
	
	printf("jukebox: Now playing \"%s\" (%f seconds)...\n", _current_track->get_name().c_str(), _current_track->get_duration_ms()/1000.0);
	
	set_recipient(new SpotifyFileWriter("output.raw"));
	
	_want_play = false;
}

void Spotify::_logged_in(sp_session *sess, sp_error error)
{
	CHECK_RESULT(error);
	Spotify *sp = reinterpret_cast<Spotify*>(sp_session_userdata(sess));
	sp_playlistcontainer *pc = sp_session_playlistcontainer(sess);
	sp_playlistcontainer_add_callbacks(pc, &sp->_pc_callbacks, sp);
	for (int i = 0; i < sp_playlistcontainer_num_playlists(pc); ++i) {
		sp->add_playlist(sp_playlistcontainer_playlist(pc, i));
    }
}

void Spotify::_logged_out(sp_session *sess)
{
	Spotify *sp = reinterpret_cast<Spotify*>(sp_session_userdata(sess));
	pthread_mutex_lock(&sp->_lock);
	pthread_cond_signal(&sp->_cond);
	pthread_mutex_unlock(&sp->_lock);
}

void Spotify::_notify_main_thread(sp_session *sess)
{
	Spotify *sp = reinterpret_cast<Spotify*>(sp_session_userdata(sess));
	pthread_mutex_lock(&sp->_lock);
	sp->_notify_do = 1;
	pthread_cond_signal(&sp->_cond);
	pthread_mutex_unlock(&sp->_lock);
}

int Spotify::_music_delivery(sp_session *sess, const sp_audioformat *format,
							 const void *frames, int num_frames)
{
	if (num_frames == 0)
        return 0; // Audio discontinuity, do nothing
	
	Spotify *sp = reinterpret_cast<Spotify*>(sp_session_userdata(sess));
	
	if (sp->_recipient)
	{
		return sp->_recipient->OnDelivery(sess, format, frames, num_frames);
	}
	else
	{
		return num_frames;
	}
}

void Spotify::_end_of_track(sp_session *sess)
{
	printf("end of track\n");
	Spotify *sp = reinterpret_cast<Spotify*>(sp_session_userdata(sess));
	if (sp->_recipient)
	{
		sp->_recipient->OnFinished();
		sp->_recipient = 0;
	}
	sp_session_player_unload(sess);
	sp->_current_track = 0;
	//sp->quit();
}

void Spotify::_metadata_updated(sp_session *sess)
{
	reinterpret_cast<Spotify*>(sp_session_userdata(sess))->try_start_playback();
}

void Spotify::_play_token_lost(sp_session *sess)
{
	printf("play token lost\n");
	Spotify *sp = reinterpret_cast<Spotify*>(sp_session_userdata(sess));
	if (sp->_current_track) {
        sp_session_player_unload(sp->_session);
        sp->_current_track = 0;
    }
}

void Spotify::_log_message(sp_session *sess, const char *msg)
{
	printf("Log: %s\n", msg);
}

void Spotify::add_playlist(sp_playlist *pl)
{
	PlaylistMap::iterator i = _playlist_map.find(pl);
	if (i == _playlist_map.end())
	{
		_playlist_map[pl] = new SpotifyPlaylist(this, pl);
	}
}

void Spotify::remove_playlist(sp_playlist *pl)
{
	PlaylistMap::iterator i = _playlist_map.find(pl);
	if (i != _playlist_map.end())
	{
		delete i->second;
		_playlist_map.erase(i);
	}
}

void Spotify::_playlist_added(sp_playlistcontainer *pc, sp_playlist *pl,
							  int position, void *userdata)
{
	reinterpret_cast<Spotify*>(userdata)->add_playlist(pl);
	
}

void Spotify::_playlist_removed(sp_playlistcontainer *pc, sp_playlist *pl,
								int position, void *userdata)
{
	reinterpret_cast<Spotify*>(userdata)->remove_playlist(pl);
}

void Spotify::_container_loaded(sp_playlistcontainer *pc, void *userdata)
{
	
}

std::vector<SpotifyPlaylist*> Spotify::get_playlists()
{
	std::vector<SpotifyPlaylist*> playlists;
	for (auto i = _playlist_map.begin(); i != _playlist_map.end(); i++)
	{
		playlists.push_back(i->second);
	}
	return playlists;
}

SpotifyPlaylist::SpotifyPlaylist(Spotify *sp, sp_playlist *pl) : _sp(sp), _pl(pl)
{
	sp_playlist_add_callbacks(_pl, &_pl_callbacks, this);
	const int num_tracks = sp_playlist_num_tracks(pl);
	_tracks.resize(num_tracks, 0);
	for (int i=0; i<num_tracks; ++i)
	{
		_tracks[i] = new SpotifyTrack(sp_playlist_track(pl, i), this, i );
	}
}

SpotifyPlaylist::~SpotifyPlaylist()
{
	for (auto track: _tracks)
		delete track;
	sp_playlist_remove_callbacks(_pl, &_pl_callbacks, this);
}

std::vector<std::string> SpotifyPlaylist::get_track_names() const
{
	std::vector<std::string> names;
	for (auto track : _tracks)
	{
		if (track)
		{
			names.push_back(track->get_name());
		}
		else
		{
			names.push_back("");
		}
	}
	return names;
}

void SpotifyPlaylist::_tracks_added(sp_playlist *pl, sp_track * const *tracks,
									int num_tracks, int position, void *userdata)
{
	SpotifyPlaylist *this_ = reinterpret_cast<SpotifyPlaylist*>(userdata);
	const int sz = position + num_tracks;
	if (this_->_tracks.size() < sz)
	{
		this_->_tracks.resize(sz, 0);
	}
	
	for (int i = 0; i < num_tracks; ++i)
	{
		this_->_tracks[position+i] = new SpotifyTrack(tracks[i], this_, position+i);
	}
	
	//if (!strcasecmp(sp_playlist_name(pl), "apptest")) {
	//	this_->_sp->set_playlist(pl);
	//	this_->_sp->try_start_playback();
	//}
}

void SpotifyPlaylist::_tracks_removed(sp_playlist *pl, const int *tracks,
									  int num_tracks, void *userdata)
{
	
}

void SpotifyPlaylist::_tracks_moved(sp_playlist *pl, const int *tracks,
									int num_tracks, int new_position, void *userdata)
{
	
}

void SpotifyPlaylist::_playlist_renamed(sp_playlist *pl, void *userdata)
{
	
}

SpotifyPlaylist *SpotifyPlaylist::_getOrCreatePlaylist(Spotify *sp, std::string name)
{
	return 0;
}

SpotifyTrack::SpotifyTrack(sp_track *track, SpotifyPlaylist *pl, int pl_index) :
_track(track),
_pl(pl),
_pl_index(pl_index)
{
	
}

SpotifyFileWriter::SpotifyFileWriter(std::string filename)
{
	_output_file = fopen(filename.c_str(), "wb");
}

SpotifyFileWriter::~SpotifyFileWriter()
{
	fclose(_output_file);
}

int SpotifyFileWriter::OnDelivery(sp_session *sess, const sp_audioformat *format,
								 const void *frames, int num_frames)
{
   size_t sz = num_frames * sizeof(int16_t) * format->channels;
   fwrite(frames, sz, 1, _output_file);
   return num_frames;
}

void SpotifyFileWriter::OnFinished()
{
	fclose(_output_file);
	delete this;
}

#endif // SPOTIFY_ENABLED
