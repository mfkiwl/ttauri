// Copyright 2019 Pokitec
// All rights reserved.

#include "TTauri/GUI/globals.hpp"
#include "TTauri/GUI/Instance.hpp"
#include "TTauri/Foundation/globals.hpp"

#include "shaders/PipelineImage.vert.spv.inl"
#include "shaders/PipelineImage.frag.spv.inl"
#include "shaders/PipelineFlat.vert.spv.inl"
#include "shaders/PipelineFlat.frag.spv.inl"
#include "shaders/PipelineMSDF.vert.spv.inl"
#include "shaders/PipelineMSDF.frag.spv.inl"


namespace TTauri::GUI {

GUIGlobals::GUIGlobals(InstanceDelegate *instance_delegate, void *hInstance, int nCmdShow) :
    instance_delegate(instance_delegate), hInstance(hInstance), nCmdShow(nCmdShow)
{
    ttauri_assert(Foundation_globals != nullptr);
    ttauri_assert(GUI_globals == nullptr);
    GUI_globals = this;

    Foundation_globals->addStaticResource(PipelineImage_vert_spv_filename, PipelineImage_vert_spv_bytes);
    Foundation_globals->addStaticResource(PipelineImage_frag_spv_filename, PipelineImage_frag_spv_bytes);
    Foundation_globals->addStaticResource(PipelineFlat_vert_spv_filename, PipelineFlat_vert_spv_bytes);
    Foundation_globals->addStaticResource(PipelineFlat_frag_spv_filename, PipelineFlat_frag_spv_bytes);
    Foundation_globals->addStaticResource(PipelineMSDF_vert_spv_filename, PipelineMSDF_vert_spv_bytes);
    Foundation_globals->addStaticResource(PipelineMSDF_frag_spv_filename, PipelineMSDF_frag_spv_bytes);
}

GUIGlobals::~GUIGlobals()
{
    delete _instance;

    ttauri_assert(GUI_globals == this);
    GUI_globals = nullptr;
}

Instance &GUIGlobals::instance()
{
    if (_instance == nullptr) {
        let lock = std::scoped_lock(mutex);
        if (_instance == nullptr) {
            ttauri_assert(instance_delegate != nullptr);
            _instance = new Instance(instance_delegate);
        }
    }
    return *_instance;
}

}
