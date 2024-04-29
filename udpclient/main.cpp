
#include "udpclient.h"
#include <filesystem>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <fstream>

using json = nlohmann::json;
namespace fs = std::filesystem;

int main(int argc, char *argv[]) 
{
    fs::path config;

    if (argc != 2)
        config = fs::path("example.json"/*path to default config*/);
    else
        config = fs::path(argv[1]);

    if(config.extension() != ".json")
        throw std::runtime_error(config.extension().string() + " configs not supported yet");

    std::ifstream file(config.string());

    json config_json = json::parse(std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()));

    // verify config
    if(!config_json["seed"].is_number_float())
        throw std::runtime_error("seed must be floating point value");
    if(!config_json["address"].is_string())
        throw std::runtime_error("address must be string value");
    if(!config_json["port"].is_number_unsigned())
        throw std::runtime_error("seed must be unsigned value");
    
    fs::path out = fs::path("output/").append(config.c_str());
    out.replace_extension(".bin");

    UDPClient client(config_json["seed"], config_json["address"], config_json["port"], out.c_str());
    
    client.waitUntilEnd();

    return 0;
}