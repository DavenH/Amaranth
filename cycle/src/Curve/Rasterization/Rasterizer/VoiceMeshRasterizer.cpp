#include <App/AppConstants.h>
#include <App/SingletonRepo.h>

#include "VoiceMeshRasterizer.h"

VoiceMeshRasterizer::VoiceMeshRasterizer(SingletonRepo* repo) :
        SingletonAccessor(repo, "VoiceMeshRasterizer")
    ,   Rasterization::VoiceRasterizer(
                repo == nullptr
                        ? Rasterization::VoiceRasterizer::defaultMinimumLineLength
                        : (float) repo->get<AppConstants>("AppConstants").getRealAppConstant(Constants::MinLineLength)) {
}
