#include "udpserver.h"
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
namespace fs = std::filesystem;

int main(int argc, char *argv[]) 
{
    fs::path config;

    if (argc != 2)
        config = fs::path("udpserver/config.json"/*path to default config*/);
    else
        config = fs::path(argv[1]);

    if(config.extension() != ".json")
        throw std::runtime_error(config.extension().string() + " configs not supported yet");

    std::ifstream file(config.string());

    json config_json = json::parse(std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()));

    // verify config
    if(!config_json["port"].is_number_unsigned())
        throw std::runtime_error("seed must be unsigned value");

    Server server(config_json["port"]);
    server.runLoop();

    return 0;
}