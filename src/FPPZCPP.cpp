
#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>
#include <cstring>

#include <istream>
#include <ostream>

#include <iostream> 

#include <filesystem>

#include <vector>

#include <unistd.h>
#include <termios.h>
#include <chrono>
#include <thread>
#include <cmath>

#include <httpserver.hpp>
#include "common.h"
#include "settings.h"
#include "Plugin.h"
#include "Plugins.h"
#include "log.h"
#include "channeloutput/channeloutput.h"
#include "mqtt.h"
#include "MultiSync.h"

#include "fppversion_defines.h"

#include "commands/Commands.h"

#include "ZCPPOutput.h"

class FPPZcppPlugin : public FPPPlugin, public httpserver::http_resource {
private:
    std::vector<std::unique_ptr <ZCPPOutput>> _zcppOutputs;
    int sequenceCount;
    Json::Value config;

public:

    FPPZcppPlugin() : FPPPlugin("fpp-zcpp-plugin"), sequenceCount(0) {
        printf ("FPPZcppPlugin Starting\n");
        readFiles();
        sendConfigFileNow();
    }
    virtual ~FPPZcppPlugin() 
    {
         _zcppOutputs.clear();
    }

    virtual const std::shared_ptr<httpserver::http_response> render_GET(const httpserver::http_request &req) override {
        std::string v = getIPs();
        return std::shared_ptr<httpserver::http_response>(new httpserver::string_response(v, 200));
    }
    
#if FPP_MAJOR_VERSION < 4 || FPP_MINOR_VERSION < 1
    virtual void modifyChannelData(int ms, uint8_t *seqData) override {
#else
    virtual void modifySequenceData(int ms, uint8_t *seqData) override {
#endif
		try
		{
			sendChannelData(seqData);
			if(sequenceCount==0)
				sendConfigFileNow();
			++sequenceCount;
			if(sequenceCount > 3000)//only resend config every 75 sec at 40fps or 140 at 20 sec, about every minute or two  
			{
				sendConfigFileNow();
				sequenceCount = 1;
			}
		}
		catch(std::exception ex)
		{
			std::cout << ex.what();
		}
    }

    virtual void playlistCallback(const Json::Value &playlist, const std::string &action, const std::string &section, int item) {
        if (settings["Start"] == "PlaylistStart" && action == "start") {
            sendConfigFileNow();
        }  
    }
    
    void sendConfigFileNow(bool sendExtra = true) {
        for(auto & output: _zcppOutputs)
        {
            printf ("Sending Config %s\n" ,output->GetIPAddress().c_str());
            output->SendConfig(sendExtra);
        }
    }

    void sendChannelData(unsigned char *data) {
        for(auto & output: _zcppOutputs)
        {
            printf ("Sending Data %s\n" ,output->GetIPAddress().c_str());
            output->SendData(data);
        }
    }
    
    void saveDataToFile()
    {
	//Save obj setting to text file for PHP gui
        std::ofstream outfile;
        outfile.open ("/home/fpp/media/config/fpp-zcpp-plugin");
        
        for(auto & out: _zcppOutputs)
        {
            outfile << out->GetIPAddress();
            outfile <<  ",";
            outfile << out->GetStartChannel();
            outfile <<  ",";
            outfile << out->GetChannelCount();
            outfile <<  "\n";
        }
        outfile.close();
    }


    std::vector<std::string> getZCPPFile(std::string const& folder)
    {
        printf ("Searching %s\n" ,folder.c_str());
        std::vector<std::string> files;
        std::string path(folder);
        std::string ext(".zcpp");
        for (auto& p : std::filesystem::recursive_directory_iterator(path))
        {
            if (p.path().extension() == ext)
            {
                files.push_back(p.path().string());
            }
        }
        return files;
    }
    
    void readFiles()
    {
        std::vector<std::string> files = getZCPPFile("/home/fpp/media/config/");
        std::vector<std::string> files2 = getZCPPFile("/home/fpp/media/upload/");
        files.insert(files.end(), files2.begin(), files2.end());
        if (files.size() > 0)
        {
	    //loop thought file list reading .zcpp files and if valid add new ZCPP obj
            for(auto const& file: files)
            {
                printf ("Reading %s\n" ,file.c_str());
                std::unique_ptr<ZCPPOutput> output = std::make_unique<ZCPPOutput>();
                bool worked = output->ReadConfig(file);
                if(worked)
                {
                    printf ("Adding Controller %s\n" ,output->GetIPAddress().c_str());
                    _zcppOutputs.push_back(std::move(output));
                }
            }
        }
        else{
            printf ("No ZCPP Configs found\n");
        }
	//read start channel settings from JSON setting file. 
	//TODO: write php web GUI to populate the JSON file
        if (LoadJsonFromFile("/home/fpp/media/config/fpp-zcpp-plugin.json", config)) {
            for(auto & out: _zcppOutputs)
            {
		//TODO: change JSON to parameter/value list
                printf ("Reading Start Channel %s\n" ,out->GetIPAddress().c_str());
                if (config.isMember(out->GetIPAddress()))
                {
                    out->SetStartChannel(config[out->GetIPAddress()].asInt());
                    printf ("Setting Start Channel %d\n",out->GetStartChannel() );
                }
                else
                {
                    printf ("No Start Channel Setting Found\n" );
                }
            }
        }
        
        saveDataToFile();
    }
    
    std::string getIPs()
    {
        std::string ips;
        for(auto & out: _zcppOutputs)
        {
            ips += out->GetIPAddress();
            ips += ",";
        }
        printf ("IP Adresses %s\n" ,ips.c_str());
        return ips;
    } 
};


extern "C" {
    FPPPlugin *createPlugin() {
        return new FPPZcppPlugin();
    }
}
