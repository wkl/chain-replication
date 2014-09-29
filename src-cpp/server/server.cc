#include <iostream>
#include <fstream>
#include <string>
using namespace std;

#include <boost/program_options.hpp>
namespace po = boost::program_options;

int main(int argc, char* argv[]) {
  try {
    po::options_description desc("Allowed options");
    desc.add_options()("help,h", "print help message")(
        "config-file,c", po::value<string>(), "specify config file path");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
      cout << desc << endl;
      return 0;
    }
    if (vm.count("config-file")) {
      ifstream ifs(vm["config-file"].as<string>());
      if (!ifs) {
        cerr << "open '" << vm["config-file"].as<string>() << "' failed"
             << endl;
        return 1;
      }
      string line;
      getline(ifs, line);
      cout << line;
    } else {
      cerr << "config-file was not set." << endl;
      return 1;
    }
  } catch (exception& e) {
    cerr << "error: " << e.what() << endl;
    return 1;
  }

  return 0;
}

