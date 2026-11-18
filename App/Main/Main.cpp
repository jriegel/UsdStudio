/**
 * \file Main.cpp
 * \date 20.2.2024
 * \author: Juergen Riegel <mail@juergen-riegel.net>
 * \copyright 2024 GPL 2.0
 * \details Headless entry point of UsdStudio. The front-end itself lives in
 *          UsdStudio::Cli::run() (Qt-free App layer) so tests can drive it
 *          with the same code path; this file only forwards argv.
 */

#include <Cli/Cli.h>

#include <iostream>

int main(int argc, char* argv[])
{
	return UsdStudio::Cli::run(argc, argv, std::cout, std::cerr);
}

