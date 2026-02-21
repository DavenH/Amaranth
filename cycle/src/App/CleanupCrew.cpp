#include "CleanupCrew.h"

CleanupCrew::CleanupCrew()
= default;

CleanupCrew::~CleanupCrew()
= default;

/*
void CleanupCrew::layerDeleted(LayerManager::LayerType type)
{
    switch (type)
    {
        case LayerManager::TimeLayer:
            surf->resetState();
            cross->resetState();
            surf->getRasterizer()->cleanUp();

            break;

        case LayerManager::FreqLayer:
            fourier2->resetState();
            fourier3->resetState();
            fourier2->getRasterizer()->cleanUp();
            break;

        case LayerManager::PhaseLayer:
            fourier2->resetState();
            fourier3->resetState();
            fourier2->getRasterizer()->cleanUp();

            break;

        case LayerManager::GuideLayer:
            guide->resetState();
            guide->getRasterizer()->cleanUp();

            break;

        case LayerManager::WaveShaperLayer:
            wave->resetState();
            wave->getRasterizer()->cleanUp();

            break;

        case LayerManager::TubeModelLayer:
            tube->resetState();
            tube->getRasterizer()->cleanUp();

            break;

        case LayerManager::EnvLayer:
            env2->resetState();
            env3->resetState();
            env2->getRasterizer()->cleanUp();

            break;

        case LayerManager::ScratchLayer:
            env2->resetState();
            env3->resetState();
            getObject(ScratchRasterizer).cleanUp();

            break;
    }
}
*/