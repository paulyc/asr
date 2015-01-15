//
//  spotify.h
//  mac
//
//  Created by Paul Ciarlo on 11/28/13.
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

#ifndef __SpotifyRipper__spotify__
#define __SpotifyRipper__spotify__

#include <iostream>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <unordered_map>
#include <exception>
#include <vector>
#include <pthread.h>
#include <string>
#include <libspotify/api.h>

extern const uint8_t g_appkey[];
extern const size_t g_appkey_size;

class Spotify;
class SpotifyPlaylist;
class SpotifyMusicRecipient;

class SpotifyTrack
{
public:
	SpotifyTrack(sp_track *track, SpotifyPlaylist *pl, int pl_index);
	virtual ~SpotifyTrack() {}
	
	sp_track *_track;
	SpotifyPlaylist *_pl;
	int _pl_index;
	
	std::string get_name() const { return std::string(sp_track_name(_track)); }
	int get_duration_ms() const { return sp_track_duration(_track); }
};

class SpotifyPlaylist
{
public:
	SpotifyPlaylist(Spotify *sp, sp_playlist *pl);
	~SpotifyPlaylist();
	
	static SpotifyPlaylist *_getOrCreatePlaylist(Spotify *sp, std::string name);
	
	std::string get_name() const { return sp_playlist_name(_pl); }
	const std::vector<SpotifyTrack*> &get_tracks() const { return _tracks; }
	std::vector<std::string> get_track_names() const;
	sp_playlist *get_sp_playlist() const { return _pl; }
	
	void set_offline(bool offline) {}
	
private:
	static void _tracks_added(sp_playlist *pl, sp_track * const *tracks,
							  int num_tracks, int position, void *userdata);
	static void _tracks_removed(sp_playlist *pl, const int *tracks,
								int num_tracks, void *userdata);
	static void _tracks_moved(sp_playlist *pl, const int *tracks,
							  int num_tracks, int new_position, void *userdata);
	static void _playlist_renamed(sp_playlist *pl, void *userdata);
	//static void _playlist_metadata_updated(sp_playlist *pl, void *userdata);
	
	sp_playlist_callbacks _pl_callbacks = {
		.tracks_added = _tracks_added,
		.tracks_removed = _tracks_removed,
		.tracks_moved = _tracks_moved,
		.playlist_renamed = _playlist_renamed,
		//	.playlist_metadata_updated = _playlist_metadata_updated
	};
	
	Spotify *_sp;
	sp_playlist *_pl;
	std::vector<SpotifyTrack*> _tracks;
};



class Spotify
{
public:
	Spotify();
	~Spotify();
	
	void login(const char *username, const char *password);
	void logout();
	void set_playlist(sp_playlist *pl) { _playlist = pl; }
	std::vector<SpotifyPlaylist*> get_playlists();
	void list_tracks();
	void play_track_index(int track_index);
	void play_track(SpotifyTrack *track);
	void try_start_playback();
	void wait_for_playback();
	void set_recipient(SpotifyMusicRecipient *recipient);
	
private:
	void run();
	void quit();
	
	void add_playlist(sp_playlist *pl);
	void remove_playlist(sp_playlist *pl);
	
	static void* _run_thread(void *arg);
	static void _logged_in(sp_session *sess, sp_error error);
	static void _logged_out(sp_session *sess);
	static void _notify_main_thread(sp_session *sess);
	static int _music_delivery(sp_session *sess, const sp_audioformat *format,
							   const void *frames, int num_frames);
	static void _end_of_track(sp_session *sess);
	static void _metadata_updated(sp_session *sess);
	static void _play_token_lost(sp_session *sess);
	static void _log_message(sp_session *sess, const char *msg);
	
	const sp_session_callbacks _session_callbacks = {
		.logged_in = _logged_in,
		.logged_out = _logged_out,
		.notify_main_thread = _notify_main_thread,
		.music_delivery = _music_delivery,
		.metadata_updated = _metadata_updated,
		.play_token_lost = _play_token_lost,
		.log_message = _log_message,
		.end_of_track = _end_of_track,
	};
	
	const sp_session_config _spconfig = {
		.api_version = SPOTIFY_API_VERSION,
		.cache_location = "tmp",
		.settings_location = "tmp",
		.application_key = g_appkey,
		.application_key_size = g_appkey_size,
		.user_agent = "spotify-paulyc",
		.callbacks = &_session_callbacks,
		.userdata = this,
		NULL,
	};
	
	static void _playlist_added(sp_playlistcontainer *pc, sp_playlist *pl,
								int position, void *userdata);
	static void _playlist_removed(sp_playlistcontainer *pc, sp_playlist *pl,
								  int position, void *userdata);
	static void _container_loaded(sp_playlistcontainer *pc, void *userdata);
	
	sp_playlistcontainer_callbacks _pc_callbacks = {
		.playlist_added = _playlist_added,
		.playlist_removed = _playlist_removed,
		.container_loaded = _container_loaded,
	};
	
	typedef std::unordered_map<sp_playlist*, SpotifyPlaylist*> PlaylistMap;
	PlaylistMap _playlist_map;
	
	// TODO can only be one session per process!
	sp_session *_session;
	pthread_t _thread;
	pthread_mutex_t _lock;
	pthread_cond_t _cond;
	int _next_timeout;
	int _notify_do;
	bool _running;
	sp_playlist *_playlist;
	SpotifyTrack *_current_track;
	
	bool _want_play;
	SpotifyMusicRecipient *_recipient;
};

class SpotifyMusicRecipient
{
public:
	virtual ~SpotifyMusicRecipient() {}
	virtual int OnDelivery(sp_session *sess, const sp_audioformat *format,
						   const void *frames, int num_frames) = 0;
	virtual void OnFinished() = 0;
};

class SpotifyFileWriter : public SpotifyMusicRecipient
{
public:
	SpotifyFileWriter(std::string filename);
	virtual ~SpotifyFileWriter();
	
	virtual int OnDelivery(sp_session *sess, const sp_audioformat *format,
						   const void *frames, int num_frames);
	virtual void OnFinished();
	
private:
	FILE *_output_file;
};

#define CHECK_RESULT(err) if (SP_ERROR_OK != (err)) { \
char error_msg[256]; \
snprintf(error_msg, sizeof(error_msg), "Runtime error: %s\n", sp_error_message(err)); \
throw std::runtime_error(error_msg); \
}

#endif /* defined(__SpotifyRipper__spotify__) */
