#include <sys/resource.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <experimental/filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <vector>

#include "yaml-cpp/yaml.h"

namespace fs = std::experimental::filesystem;

const auto HASH_REGEX = std::regex("#");

enum class Status { AC, WA, RE, TLE, MLE, OLE, CE, UK };

struct ProgramRunningInfo {
  uint32_t time_usage;
  uint32_t memory_usage;
  Status status;
};

std::string status_to_str(Status status) {
  switch (status) {
    case Status::AC:
      return "Accepted";

    case Status::WA:
      return "Wrong Answer";

    case Status::RE:
      return "Runtime Error";

    case Status::TLE:
      return "Time Limit Exceeded";

    case Status::MLE:
      return "Memory Limit Exceeded";

    case Status::OLE:
      return "Output Limit Exceeded";

    default:
      throw;
  }
}

std::string compile(const std::string &code_filepath, const std::string &lang) {
  auto output_filepath =
      fs::path(code_filepath).replace_filename("exe").string();

  if (lang == "c") {
    system(
        ("gcc -Wall -std=c11 -lm -o " + output_filepath + " " + code_filepath)
            .c_str());
  } else if (lang == "c++") {
    system(
        ("g++ -Wall -std=c++14 -lm -o " + output_filepath + " " + code_filepath)
            .c_str());
  } else {
    // std::cout << "Unsupported language" << std::endl;
  }

  return output_filepath;
}

ProgramRunningInfo run_program(const std::string &program_filepath,
                               const std::string &input_filepath,
                               const std::string &user_output_filepath,
                               uint32_t time_limit, uint32_t memory_limit,
                               uint32_t output_limit) {
  ProgramRunningInfo info;
  pid_t pid;

  switch (pid = fork()) {
    case -1:
      // std::cout << "Fork failed" << std::endl;
      break;

    case 0: {
      struct rlimit rl;

      rl.rlim_cur = rl.rlim_max = time_limit;
      setrlimit(RLIMIT_CPU, &rl);

      rl.rlim_cur = rl.rlim_max = memory_limit * 1024;
      setrlimit(RLIMIT_AS, &rl);

      rl.rlim_cur = rl.rlim_max = output_limit * 1024;
      setrlimit(RLIMIT_FSIZE, &rl);

      struct itimerval itv;
      itv.it_interval.tv_sec = itv.it_interval.tv_usec = 0;
      itv.it_value.tv_sec = time_limit * 1.25 / 1000;
      itv.it_value.tv_usec = (uint32_t)(time_limit * 1.25 * 1000) % 1000000;
      setitimer(ITIMER_REAL, &itv, nullptr);

      freopen(input_filepath.c_str(), "r", stdin);
      freopen(user_output_filepath.c_str(), "w", stdout);

      char *argv[] = {nullptr};
      char *envp[] = {nullptr};
      execve(program_filepath.c_str(), argv, envp);
    } break;

    default:
      int status;
      struct rusage rusage;

      wait4(pid, &status, 0, &rusage);

      info.status = Status::UK;
      info.time_usage = rusage.ru_utime.tv_sec * (uint32_t)1000 +
                        rusage.ru_utime.tv_usec / 1000 +
                        rusage.ru_stime.tv_sec * (uint32_t)1000 +
                        rusage.ru_stime.tv_usec / 1000;
      info.memory_usage = rusage.ru_maxrss;

      if (info.memory_usage > memory_limit) {
        info.status = Status::MLE;
        // std::cout << "MLE" << std::endl;
      } else if (WIFSIGNALED(status)) {
        auto signum = WTERMSIG(status);

        switch (signum) {
          case SIGXCPU:
          case SIGALRM:
            info.status = Status::TLE;
            // std::cout << "TLE" << std::endl;
            break;
          case SIGABRT:
          case SIGILL:
          case SIGBUS:
          case SIGFPE:
          case SIGSEGV:
            info.status = Status::RE;
            // std::cout << "RE" << std::endl;
            break;
          case SIGXFSZ:
            info.status = Status::OLE;
            // std::cout << "OLE" << std::endl;
          default:
            break;
        }
      }

      break;
  }

  return info;
}

bool std_check(const std::string &user_output_filepath,
               const std::string &answer_filepath) {
  auto user_output_stream = std::ifstream(user_output_filepath);
  auto answer_stream = std::ifstream(answer_filepath);

  int out, ans;
  bool is_accepted = true;

  while (true) {
    out = user_output_stream.get();
    ans = answer_stream.get();

    if (out == EOF || ans == EOF) {
      if (out != ans) {
        is_accepted = false;
      }
      break;
    }

    if (out != ans) {
      is_accepted = false;
      break;
    }
  }

  user_output_stream.close();
  answer_stream.close();

  return is_accepted;
}

int main(int argc, char *argv[]) {
  if (argc != 4) {
    std::cout << "Usage: " << argv[0]
              << " [submitted source code file] [language] [test set directory]"
              << std::endl;
    return 0;
  }

  auto code_filepath = std::string(argv[1]);
  auto lang = std::string(argv[2]);
  auto test_set_dirpath = std::string(argv[3]);

  auto program_filepath = compile(code_filepath, lang);

  auto test_set_config_filepath =
      fs::path(test_set_dirpath).append("config.yml");
  auto config_node = YAML::LoadFile(test_set_config_filepath);
  auto time_limit = config_node["timeLimit"].as<uint32_t>();
  auto memory_limit = config_node["memoryLimit"].as<uint32_t>();
  auto test_points_node = config_node["testPoints"];
  auto input_basename = config_node["inputFile"].as<std::string>();
  auto output_basename = config_node["outputFile"].as<std::string>();

  YAML::Node result_node;
  double total_score = 0.0;
  uint32_t total_time_usage = 0;
  uint32_t memory_usage_peak = 0;
  Status overall_status = Status::AC;

  for (auto &&test_point_node : test_points_node) {
    auto score = test_point_node["score"].as<double>();

    for (auto &&case_node : test_point_node["cases"]) {
      auto input_filename = std::regex_replace(input_basename, HASH_REGEX,
                                               case_node.as<std::string>());
      auto input_filepath = fs::path(test_set_dirpath)
                                .append("input")
                                .append(input_filename)
                                .string();

      auto user_output_filepath = fs::path(code_filepath)
                                      .replace_filename(input_filename)
                                      .replace_extension("out")
                                      .string();

      auto answer_filename = std::regex_replace(output_basename, HASH_REGEX,
                                                case_node.as<std::string>());
      auto answer_filepath = fs::path(test_set_dirpath)
                                 .append("output")
                                 .append(answer_filename)
                                 .string();

      auto info =
          run_program(program_filepath, input_filepath, user_output_filepath,
                      time_limit, memory_limit, 65535);

      if (info.status == Status::UK) {
        auto is_accepted = std_check(user_output_filepath, answer_filepath);

        if (is_accepted) {
          total_score += score;
          info.status = Status::AC;
        } else {
          info.status = Status::WA;

          if (overall_status == Status::AC) {
            overall_status = Status::WA;
          }
        }
      } else if (overall_status == Status::AC || overall_status == Status::WA) {
        overall_status = info.status;
      }

      total_time_usage += info.time_usage;
      memory_usage_peak = std::max(memory_usage_peak, info.memory_usage);

      YAML::Node result_test_point_node;
      result_test_point_node["status"] = status_to_str(info.status);
      result_test_point_node["score"] = info.status == Status::AC ? score : 0;
      result_test_point_node["timeUsage"] = info.time_usage;
      result_test_point_node["memoryUsage"] = info.memory_usage;

      result_node["testPoints"].push_back(result_test_point_node);
    };
  }

  YAML::Emitter out;
  result_node["status"] = status_to_str(overall_status);
  result_node["score"] = total_score;
  result_node["timeUsage"] = total_time_usage;
  result_node["memoryUsage"] = memory_usage_peak;
  out << result_node;
  std::cout << out.c_str() << std::endl;

  return 0;
}