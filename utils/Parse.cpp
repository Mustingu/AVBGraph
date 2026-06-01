#include "Parse.h"

void Args::ParseArgs(int argc, char** argv) {
  std::string preset_input_file_key = "";
  std::set<std::string> preset_input_file_names;
  for (const auto& pair : PreFileMap) {
    preset_input_file_names.insert(pair.first);
  }

  app.add_flag("-g,--gen-and-output", gen_and_output, "可选：生成并输出文件");
  app.add_flag("--read", read_test, "可选：测试读");
  app.add_option("-r, --read-thread", read_thread, "可选：读线程数量, 默认64");
  app.add_option("--write-thread", Spruce_wt, "可选：写线程数量，默认64");
  app.add_option("-e,--edge-limit", limit_edge_nums,
                 "可选：限制边数,默认或0时不限制");
  app.add_flag("-p,--permute", permute_, "可选：输入边集乱序");
  app.add_flag("-c,--check", check_, "可选：检查结果");
  app.add_flag("--con", con_wr_test, "可选：并发读写");
  app.add_flag("-s,--single", single_read_work,
               "可选：单任务多线程读，默认不开启");
  app.add_option("--bfs-root", bfsroot, "可选：BFS起始点");

  app.add_option("-a,--algorithm", algorithm_, "可选：算法,支持:sum");
  app.add_flag("--csr", csr, "可选：CSRBaseline");

  auto* group = app.add_option_group("输入文件互斥");
  auto* prefile_opt =
      group
          ->add_option("--prefile", preset_input_file_key,
                       "可选:预设输入文件, 支持以下键名:dota/graph24")
          ->check(CLI::IsMember(
              preset_input_file_names));  // 检查用户输入是否在预设列表中
  group->add_option("-f,--file", input_file, "可选：输入.e文件");
  group->require_option(0, 1);

  app.callback([&]() {
    // 只有当用户提供了 --prefile 选项时，我们才执行 map 查找逻辑
    if (prefile_opt->count() > 0) {
      auto it = PreFileMap.find(preset_input_file_key);
      if (it != PreFileMap.end()) {
        input_file = it->second;
      }
    }
  });

  try {
    app.parse(argc, argv);
  } catch (const CLI::ParseError& e) {
    app.exit(e);
  }
  if (!input_file.empty()) {
    file_input = true;
  }
}