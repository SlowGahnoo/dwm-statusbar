#include <iostream> // std::cout, std::ostream
#include <iomanip>  // std::fixed, std::setprecision, std::setw
#include <fstream>
#include <memory>   // std::shared_ptr, std::make_shared
#include <thread>   // std::this_tread::sleep_for
#include <chrono>   // std::chrono::seconds, std::chrono::minutes, std::chrono::duration_cast
#include <array>
#include <cmath>

#include <unistd.h> // close

#include <X11/Xlib.h>
#include <X11/XKBlib.h>

#include <linux/wireless.h> // wreq, IW_...
#include <sys/ioctl.h>      // ioctl
#include <sys/socket.h>     // socket
#include <string.h>         // memset

#include <fmt/core.h>
#include <fmt/chrono.h>

#define COLOR_FMT(_fcolor, _fvar) "^c" _fcolor "^" _fvar "^d^"

// #define COLOR_NORMAL "#BD93F9"
#define COLOR_NORMAL "#FFFFFF"
#define COLOR_ORANGE "#FFA500"
// #define COLOR_RED    "#FF0000"
#define COLOR_YELLOW "#F1FA8C"
#define COLOR_BLUE   "#8BE9FD"
#define COLOR_GREEN  "#50FA7B"
#define COLOR_RED    "#FF5555"

const auto TIME_INTERVAL_MS = 500;

std::string
byte_human_fmt(uint64_t bytes)
{
	constexpr auto suffix = std::to_array({"", "Ki", "Mi", "Gi", "Ti"});
	constexpr unsigned base = 1024;
	constexpr auto sz = std::size(suffix);
	auto i = sz;

	auto scaled = static_cast<double>(bytes);
	while (--i && (scaled /= base) >= base);

	return fmt::format("{:.1f}{}", scaled, suffix[sz + 1 - i]);
}

#include <boost/range/adaptor/reversed.hpp>
#include <boost/locale.hpp>
std::u32string
scroll_str(std::string& s)
{
	auto str = boost::locale::conv::utf_to_utf<char32_t>(s);
	auto tmp = str.front();
	for (auto& chr : boost::adaptors::reverse(str)) {
		std::swap(tmp, chr);
	}
	s = boost::locale::conv::utf_to_utf<char>(str);
	return str;
}


#include "module.hpp"

class WIFI : public Module {
	const char* IW_INTERFACE = "wlp3s0";
	const char* icon = COLOR_FMT(COLOR_NORMAL, " ");
	struct iwreq wreq;
	char ssid[IW_ESSID_MAX_SIZE + 1];
	int sockfd;

	std::ostream&
	update(std::ostream& out)
	{
		memset(&wreq, 0, sizeof(struct iwreq));
		strcpy(wreq.ifr_name, IW_INTERFACE);
		wreq.u.essid.pointer = ssid;
		wreq.u.essid.length = IW_ESSID_MAX_SIZE + 1;
		sockfd = socket(AF_INET, SOCK_DGRAM, 0);

		if (ioctl(sockfd, SIOCGIWESSID, &wreq) == -1)
			std::cerr << "Get ESSID ioctl failed" << std::endl;
		close(sockfd);
		if (!strcmp(ssid, ""))
			strcpy(ssid, "Offline");

		out << icon << ssid;
		return out;
	}
};

class Temperature : public Module {
	static constexpr auto temp_icon = std::to_array ({
			COLOR_FMT(COLOR_NORMAL, ""),
			COLOR_FMT(COLOR_NORMAL, ""),
			COLOR_FMT(COLOR_ORANGE, ""),
			COLOR_FMT(COLOR_RED,    "")
		});

	static constexpr unsigned max_temp = 105000; // 105 degrees for intel i5-3320m
	std::ifstream temp0;
	unsigned temp;

public:
	Temperature() : temp0("/sys/class/thermal/thermal_zone0/temp"), temp(0) {}
private:
	std::ostream&
	update(std::ostream& out)
	{
		temp0 >> temp;
		auto index = static_cast<uint>(ceil(0.027 * temp / 1000));
		out << temp_icon[index] << " " << temp/1000 << "°C";
		temp0.clear();
		temp0.seekg(0);
		return out;
	}
};

class Battery : public Module {
	std::ifstream AC, CAP;
	static constexpr auto bat_icon = std::to_array({
			COLOR_FMT(COLOR_RED,    "  "),
			COLOR_FMT(COLOR_NORMAL, "  "),
			COLOR_FMT(COLOR_NORMAL, "  "),
			COLOR_FMT(COLOR_NORMAL, "  "),
			COLOR_FMT(COLOR_NORMAL, "  ")
	});
	// ANIMATION_SPEED has a range of (0.0, 1.0)
	static constexpr auto ANIMATION_SPEED = 1.0;
	unsigned animation_frame = 1;
	double t = 0.0;
	// const char* ac_icon = COLOR_FMT(COLOR_BLUE, "");
	unsigned status, capacity;

public:
	Battery() :
		AC ("/sys/class/power_supply/AC/online"),
		CAP("/sys/class/power_supply/BAT0/capacity") {}
private:
	std::ostream&
	update(std::ostream& out)
	{
		AC >> status; CAP >> capacity;
		if (status && capacity < 100) {
			t += TIME_INTERVAL_MS * ANIMATION_SPEED;	
			if (t > TIME_INTERVAL_MS) {
				animation_frame++;
				t = 0;
			}
			out << bat_icon[(animation_frame < 5) ? animation_frame : (animation_frame = 1)]
				<< capacity << '%';
		} else 
			out << bat_icon[capacity / 25] <<  capacity << '%';

		AC.clear(); CAP.clear();
		AC.seekg(0); CAP.seekg(0);
		return out;
	}
};

class Memory : public Module {
	std::ifstream meminfo;
	uint64_t total, free, buffers, cached;
	std::string token;
	static constexpr auto icon = COLOR_FMT(COLOR_NORMAL, " ");

public:
	Memory() : meminfo("/proc/meminfo") ,
			   total(0), free(0), buffers(0), cached(0) {}
	std::ostream&
	update(std::ostream& out)
	{
		while (meminfo >> token) {
			if (token == "MemTotal:")
				meminfo >> total;
			if (token == "MemFree:")
				meminfo >> free;
			if (token == "Buffers:")
				meminfo >> buffers;
			if (token == "Cached:") {
				meminfo >> cached;
				break;
			}
		}
		meminfo.clear(); meminfo.seekg(0);
		out << icon << byte_human_fmt(total - free - buffers - cached);
		return out;
	}
};

class DateTime : public Module {
	time_t t;
	struct tm* tm;
	static constexpr auto hours = std::to_array({
		"🕛", "🕐", "🕑", "🕒", "🕓", "🕔",
		"🕕", "🕖", "🕗", "🕘", "🕙", "🕚"
		});

	static constexpr auto thirty = std::to_array({
		"🕧", "🕜", "🕝", "🕞", "🕟", "🕠",
		"🕡", "🕢", "🕣", "🕤", "🕥", "🕦"
	});
	std::string datetime;

	std::string
	emoji_clock()
	{
		unsigned index = static_cast<unsigned>(tm->tm_hour) % 12;
		return fmt::format(COLOR_FMT(COLOR_NORMAL, "{}"), ((tm->tm_min < 30) ? hours[index] : thirty[index]));
	}

	std::ostream&
	update(std::ostream& out)
	{
		t = time(NULL);
		tm = localtime(&t);
		datetime = fmt::format("{}{:%H:%M " COLOR_FMT(COLOR_NORMAL, " ") "%A %d %B} ", emoji_clock(), *tm);
		out <<  datetime;
		return out;
	}
};

class Thinklight : public Module {
	std::ifstream thinklight;
	bool state;
public:
	Thinklight() : thinklight("/sys/devices/platform/thinkpad_acpi/leds/tpacpi::thinklight/brightness"),
				   state(0) {}
private:
	std::ostream&
	update(std::ostream& out)
	{
		thinklight >> state;
		out << ((state) ? " Thinklight" : "");
		thinklight.clear(); thinklight.seekg(0);
		return out;
	}
};

#include <mpd/client.h>
#include <mpd/status.h>

class MPD : public Module {
	struct mpd_connection* conn;
	struct mpd_status* status;
	struct mpd_song* song;
	static constexpr auto MAX_LEN = 30;
	static constexpr auto icons = std::to_array({
		/* MPD_STATE_UNKNOWN */ "",
		/* MPD_STATE_STOP    */ COLOR_FMT(COLOR_GREEN, " "),
		/* MPD_STATE_PLAY    */ COLOR_FMT(COLOR_GREEN, " "),
		/* MPD_STATE_PAUSE   */ COLOR_FMT(COLOR_GREEN, " ")
		});
	unsigned elapsed = 0, total = 0, state = 0;
	std::string duration;
	std::string scrll;
	std::string tmp1;
	std::string tmp2;

	std::ostream&
	print_tag(enum mpd_tag_type type, std::ostream& out)
	{
		unsigned i = 0;
		const char* value;
		while ((value = mpd_song_get_tag(song, type, i++)) != NULL)
			out << value;
		return out;
	}

	void
	change_name()
	{
		int sz = MAX_LEN - static_cast<signed>(tmp1.size());
		scrll = (sz > 0) ? std::string(MAX_LEN - tmp1.size(), ' ') + tmp1 : std::string(10,' ') + tmp1;
	}

	std::ostream&
	update(std::ostream& out)
	{
		conn = mpd_connection_new(NULL, 0, 30000);
		if (mpd_connection_get_error(conn) != MPD_ERROR_SUCCESS) {
			goto free_connection;
		}
		mpd_command_list_begin(conn, true);
		mpd_send_status(conn);
		mpd_send_current_song(conn);
		mpd_command_list_end(conn);

		if ((status = mpd_recv_status(conn))) {

			if ((state = mpd_status_get_state(status)) == MPD_STATE_STOP) {
				out << icons[state] << "Empty Playlist"; 
			} else {
				mpd_response_next(conn);

				while ((song = mpd_recv_song(conn)) != NULL) {
					auto artist = mpd_song_get_tag(song, MPD_TAG_ARTIST, 0);
					auto title = mpd_song_get_tag(song, MPD_TAG_TITLE, 0);
					auto s_artist = ((artist) ? std::string(artist) + " - " : "");
					auto s_title = ((title) ? std::string(title) : "");
					tmp1 = fmt::format("{} {}", s_artist, s_title);
					if (tmp1 != tmp2) {
						change_name();
					}
					tmp2 = fmt::format("{} {}", s_artist, s_title);

					elapsed = mpd_status_get_elapsed_time(status);
					total = mpd_status_get_total_time(status);

					if (mpd_song_get_duration(song) > 0) {
						duration = fmt::format(" [{}:{:02}/{}:{:02}]",
												elapsed/60, elapsed%60, total/60, total%60);
					} else duration.clear();
					mpd_song_free(song);
			}
				auto tmp = scroll_str(scrll);
				auto s = boost::locale::conv::utf_to_utf<char>(tmp.substr(0, MAX_LEN));
				out << icons[state] << s << duration;
			}
		} else {
			goto free_status;
		}

		mpd_status_free(status);
		mpd_connection_free(conn);
		return out;

	free_status:
		mpd_status_free(status);
	free_connection:
		mpd_connection_free(conn);
		return out;
	}
};

class Penguin : public Module {
	static constexpr auto penguin = COLOR_FMT(COLOR_NORMAL , " ");

	std::ostream&
	update(std::ostream& out) {
		out << penguin;
		return out;
	}
};


#include <csignal>
#include <algorithm>
#include <iterator>
static Display* dpy;
static bool running = true;
#include "config.hpp"

template<typename ... T>
void die(const T& ... args)
{
	((std::cerr << args << ' '), ...);
	exit(EXIT_FAILURE);
}

void quit(int)
{
	running = false;
}

void terminate(int)
{
	running = false;
	XStoreName(dpy, DefaultRootWindow(dpy), "Terminated");
	XSync(dpy, False);
}


void bar_update(const std::stringstream& st)
{
	XStoreName(dpy, DefaultRootWindow(dpy), st.str().data());
	XSync(dpy, False);
}


void run(void)
{

	while (running) {
		std::stringstream status;
		for (const auto& module : modules) {
			status << *module << delim;
		}
		bar_update(status);
		std::this_thread::sleep_for(std::chrono::milliseconds(TIME_INTERVAL_MS));
	}
}


int main(void)
{
	if (!(dpy = XOpenDisplay(NULL))) {
		die("Cannot open display");
	}
	signal(SIGINT,  terminate);
	signal(SIGTERM, terminate);
	run();
	XSync(dpy, False);
	XCloseDisplay(dpy);
	return EXIT_SUCCESS;
}
