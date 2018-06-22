// Copyright 2016-2018 Doug Moen
// Licensed under the Apache License, version 2.0
// See accompanying file LICENSE or https://www.apache.org/licenses/LICENSE-2.0

#include <libcurv/geom/viewer.h>
#include <glslviewer.h>
#include <iostream>
#include <fstream>
#include <libcurv/string.h>
#include <libcurv/geom/export_frag.h>
#include <libcurv/geom/tempfile.h>
extern "C" {
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/unistd.h>
#include <sys/wait.h>
#include <signal.h>
}

namespace curv { namespace geom {

void
run_glslviewer_interrupt_handler(int)
{
    remove_all_tempfiles();
    exit(1);
}

void
run_glslviewer(int argc, const char** argv)
{
    pid_t pid = fork();
    if (pid == 0) {
        // in child process
        int fd = open("/dev/null", O_WRONLY);
        dup2(fd, 1); // TODO: suppress unwanted messages written to stdout
        exit(glslviewer_main(argc, argv));
    } else if (pid == pid_t(-1)) {
        std::cerr << "can't fork Viewer process: " << strerror(errno) << "\n";
    } else {
        struct sigaction interrupt_action;
        memset((void*)&interrupt_action, 0, sizeof(interrupt_action));
        interrupt_action.sa_handler = run_glslviewer_interrupt_handler;
        sigaction(SIGINT, &interrupt_action, nullptr);

        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
            ;
        else {
            std::cerr << "Viewer process abnormal exit: " << status << "\n";
        }
    }
}

// Open a Viewer window displaying shape, and block until the window is closed.
void
run_viewer(Shape_Recognizer& shape)
{
    auto fragname = make_tempfile(".frag");
    std::ofstream f(fragname.c_str());
    export_frag(shape, f);
    f.close();

    const char* argv[3];
    argv[0] = "curv";
    argv[1] = fragname.c_str();
    argv[2] = nullptr;
    run_glslviewer(2, argv);
}

pid_t viewer_pid = pid_t(-1);

void
poll_viewer()
{
    if (viewer_pid != pid_t(-1)) {
        int status;
        pid_t pid = waitpid(viewer_pid, &status, WNOHANG);
        if (pid == viewer_pid) {
            if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
                ;
            else {
                std::cerr << "Viewer process abnormal exit: " << status << "\n";
            }
            viewer_pid = pid_t(-1);
        }
    }
}

void
launch_viewer(boost::filesystem::path filename)
{
    poll_viewer();
    if (viewer_pid == pid_t(-1)) {
        pid_t pid = fork();
        if (pid == 0) {
            // in child process
            const char* argv[3];
            argv[0] = "curv";
            argv[1] = filename.c_str();
            argv[2] = nullptr;
            exit(glslviewer_main(2, argv));
        } else if (pid == pid_t(-1)) {
            std::cerr << "can't fork Viewer process: "
                      << strerror(errno) << "\n";
        } else {
            viewer_pid = pid;
        }
    }
}

void
open_viewer(Shape_Recognizer& shape)
{
    auto filename = make_tempfile(".frag");
    std::ofstream f(filename.c_str());
    export_frag(shape, f);
    f.close();
    launch_viewer(filename);
}

void
close_viewer()
{
    if (viewer_pid != (pid_t)(-1))
        kill(viewer_pid, SIGTERM);
}

}} // namespace