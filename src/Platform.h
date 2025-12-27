//============================================================================
// Name        : BoxyLady
// Author      : Darren Green
// Copyright   : (C) Darren Green 2011-2025
// Description : Music sequencer
//
// License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>
// This is free software; you are free to change and redistribute it.
// There is NO WARRANTY, to the extent permitted by law.
// Contact: darren.green@stir.ac.uk http://pinkmongoose.co.uk
//============================================================================

#ifndef PLATFORM_H_
#define PLATFORM_H_

#include <string>
#include <map>
#include <filesystem>

#ifdef __GNUC__
#define BOXY_COMPILER "g++"
#elifdef __clang__
#define BOXY_COMPILER "Clang"
#elifdef _MSC_VER
#define BOXY_COMPILER "Visual Studio"
#else
#define BOXY_COMPILER "unknown compiler"
#endif

#ifdef _WIN32
#define BOXY_PLATFORM "Windows"
#elifdef __APPLE__
#define BOXY_PLATFORM "Apple"
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__sun) || defined(_AIX)
#define BOXY_PLATFORM "Unix"
#else
#define BOXY_PLATFORM "Unknown"
#endif

class Platform {
private:
struct OSDetails {
	std::string_view UserConfigEnvVar, AppConfigSubDir, BootLibrary, DefaultShellCommand;
};
inline static const std::map<std::string_view, OSDetails> oses_ {
	{"Windows",{"APPDATA","BoxyLady","Boot.box","echo"}},
	{"Apple",{"HOME","Library/Application Support/BoxyLady","Boot.box","echo"}},
	{"Unix",{"HOME",".config/BoxyLady","Boot.box","echo"}},
	{"Unknown",{"","","Boot.box","echo"}}
};
std::filesystem::path app_config_dir_ {};
std::string boot_library_ {}, default_shell_command_ {};
public:
inline static constexpr std::string_view CompileDate {__DATE__}, CompileTime {__TIME__},
	Compiler {BOXY_COMPILER}, PlatformName {BOXY_PLATFORM};
explicit Platform() {
	const OSDetails os = oses_.at(PlatformName);
	const char* user_config_dir {getenv(os.UserConfigEnvVar.data())};
	app_config_dir_ = user_config_dir?
		std::filesystem::path{user_config_dir} / os.AppConfigSubDir:
		std::filesystem::current_path();
	boot_library_ =  os.BootLibrary;
	default_shell_command_ = os.DefaultShellCommand;
	if (!std::filesystem::exists(app_config_dir_))
		std::filesystem::create_directories(app_config_dir_);
}
std::filesystem::path AppConfigDir() const {return app_config_dir_;}
std::string_view BootLibrary() const {return boot_library_;}
std::string_view DefaultShellCommand() const {return default_shell_command_;}
};

#endif /* PLATFORM_H_ */
