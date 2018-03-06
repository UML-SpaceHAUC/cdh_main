#ifndef _OCTOPOS_DRIVER_H_
#define _OCTOPOS_DRIVER_H_

#include <string>
#include <utility>
#include <ctime>
#include <map>
#include <iostream>
#include <cstdlib>
#include <cstring> // for memset
#include <sys/types.h>
#include <sys/wait.h>
#include <queue>

#include <octopOS/publisher.h>
#include "json.hpp" // TODO(llazarek): Replace with real lib

#include "Optional.hpp"

extern const char*  CONFIG_PATH;
extern const char*  UPGRADE_TOPIC;
extern const char*  DOWNGRADE_TOPIC;
extern const time_t RUNTIME_CUTOFF_DOWNGRADE_S;
extern const int    DEATH_COUNT_CUTOFF_DOWNGRADE;
extern const bool   LISTEN_FOR_MODULE_UPGRADES;

typedef long MemKey;
struct Module {
    pid_t pid;
    MemKey msgkey;
    bool killed, downgrade_requested;
    time_t launch_time;
    int early_death_count;
    Module(pid_t _pid, MemKey _msgkey, time_t _launch_time):
	pid(_pid), msgkey(_msgkey), launch_time(_launch_time),
	killed(false), downgrade_requested(false),
	early_death_count(0) { }
    Module() { } // c++ STL Map wants default constructor
};
typedef std::map<std::string, Module> ModuleInfo;
typedef std::pair<ModuleInfo, MemKey> LaunchInfo;
typedef std::string FilePath;

bool accessible(FilePath file);
Optional<json> load(FilePath json_file);
pid_t launch(FilePath module, MemKey key);
void relaunch(Module &module, FilePath path);
LaunchInfo launch_modules_in(FilePath dir, MemKey start_key);
std::list<FilePath> modules_in(FilePath dir);
Optional< std::list<FilePath> > files_in(FilePath dir);
bool launch_octopOS_listeners();
Optional<std::string> find_module_with(pid_t pid, const ModuleInfo &modules);
void reboot_module(std::string path, ModuleInfo *modules,
		   publisher<std::string> &downgrade_pub);
int kill_module(std::string path, ModuleInfo *modules);
bool module_needs_downgrade(Module *module);
void babysit_forever(ModuleInfo &modules);


class ChildHandler {
public:
    static void sigchld_handler(int sig)
    {
	pid_t pid;
	int status;
	pthread_t tmp;

	while ((pid = waitpid(-1, &status, WNOHANG)) != -1) {
	    // handle all dead children later
	    rebootQ.push(pid);
	}
    }

    static void register_child_handler(ModuleInfo *_modules,
				       publisher<std::string> *_downgrade_pub) {
	modules = *_modules;
	downgrade_pub = _downgrade_pub;
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sigchld_handler;

	sigaction(SIGCHLD, &sa, NULL);
    }

    static void reboot_dead_modules() {
	while(!rebootQ.empty()) {
	    pid_t pid = rebootQ.front();
	    rebootQ.pop();
	    Optional<std::string> found = find_module_with(pid, modules);
	    if (found.isEmpty()) {
		std::cerr << "Notification of unregistered module death. "
			  << "Something has probably gone horribly wrong."
			  << std::endl;
	    } else {
		reboot_module(found.get(), &modules, *downgrade_pub);
	    }
	}
    }

private:
    static ModuleInfo &modules;
    static publisher<std::string> *downgrade_pub;
    static std::queue<pid_t> rebootQ;
};

#endif /* _OCTOPOS_DRIVER_H_ */
