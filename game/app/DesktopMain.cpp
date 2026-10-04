#include "game/app/OpenYammMain.h"

#include <SDL3/SDL.h>

#include <exception>
#include <iostream>

int main(int argc, char **argv)
{
    try
    {
        SDL_SetAppMetadata("OpenYAMM", nullptr, "io.github.openyamm.OpenYAMM");
        return OpenYAMM::Game::runApplication(argc, argv);
    }
    catch (const std::exception &exception)
    {
        std::cerr << "Fatal error: " << exception.what() << '\n';
        return 1;
    }
}
