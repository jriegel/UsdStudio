/**
 * \file Main.cpp
 * \date 20.2.2024
 * \author: Juergen Riegel <mail@juergen-riegel.net>
 * \copyright 2024 GPL 2.0
 * \details This file holds the executable definition to dump Jt information as JSON
 */

#include <CLI/CLI.hpp>

#

#include <iostream>
#include <iomanip>
#include <assert.h>
#include <regex>

//#include <Matrix.h>

using namespace std;



int main(int argc, char* argv[])
{
	CLI::App app("UsdStudioCmd - Command line tool of UsdStdudio");

	// add version output
	app.set_version_flag("--version", std::string("0.8"));

	std::vector<std::string> files;
	app.add_option("files", files, "Jt file(s) to dump")->check(CLI::ExistingFile);
    std::string outFileName;
    app.add_option("-o", outFileName, "Out File Name");


	CLI11_PARSE(app, argc, argv);

	if (files.size() == 0) {

		cerr << "No input  files given!" << endl;
		return 1;
	}

    std::ofstream outFile(outFileName.c_str());
    if (outFile.is_open()) {
        
    }

    

	// go through Jt files
	for (const auto& aarg : files) {

       
		
	}


	return 0;
}

