
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <map>
#include <string>

#include <OpenColorIO/OpenColorIO.h> //NOLINT

#include "xstudio/utility/json_store.hpp"
#include "xstudio/colour_pipeline/colour_pipeline.hpp"

namespace OCIO = OCIO_NAMESPACE;


namespace xstudio::colour_pipeline::ocio {


/**
 * ShaderBuilder provide OCIO shader generation and runtime utilities.
 *
 * Optionally, CDL transforms, as used in typical grading scenarios can
 * be automatically turned into uniforms baked code in the generated shader.
 * This is used in xStudio to avoid generating new shader code for every shots
 * we encounter and is particulaly helpful when playback reach shot boundaries.
 * While xStudio generate OCIO transforms ahead of time, shader compilation is
 * currently performed in the main GL thread, just before viewing when needed.
 * Compilation can take roughly 200-300ms and this results in a noticeable
 * freeze on transitions.
 *
 * OCIO itself do not offer any mechanism to automatically turn CDL into
 * dynamic transforms (using DynamicProperties). GradingPrimaryTransform
 * allows to implement CDLs and can be dynamic, but direct conversion from CDL
 * is not possible using the OCIO API, nor does OCIO support more than 1
 * dynamic property of the same type per shader (which can be needed if
 * an OCIO view use more than 1 grade).
 *
 * The dynamic CDL behaviour is controlled by using the ``setDynamicCDLs()``
 * and ``setDynamicCDLMode()`` setters and overall is driven by the media hook
 * metadatas.
 *
 * Two modes are currently available to perform this task:
 * * Config mode, will create a copy of the OCIO config where every FileTransform
 *   identified as a dynamic CDL will be replaced by a GradingPrimaryTransform,
 * * Processor mode, will use the OCIO config as-is but will iterate through all
 *   transforms in the generated OCIO processor and replace CDLTransform identified
 *   as dynamic CDL. This relies on FormatMetadata (name attribute) available in
 *   the CDLTransform to correctly identify the CDLTransform as being one of the
 *   dynamic CDL as opposed to a CDLTransform used in the OCIO config for other
 *   purposes.
 *
 * The processor mode is more robust, for example CDLs can be embedded in CLF
 * files, which can include additional processing like colour space conversion.
 * With the Config mode the entire file would be replaced by a single
 * GradingPrimaryTransform and these would be lost.
 *
 * GradingPrimaryTransform generated above get tracked by ID (using the offset)
 * and the final shader code is patched to replace these by code using uniforms
 * to apply the CDL. Not the use of GradingPrimaryTransform for this process
 * is not an absolute necessity, but at least this prevents OCIO from optimizing
 * these away and may simplify porting the code when OCIO offer better support
 * for this workflow.
 *
 * At runtime, the uniforms values for these CDL are updated accordingly.
 *
 */


struct DynamicCDL {
    int id{-1};
    bool in_use{false};
    std::string look_name;
    std::string file_name;
    std::string resolved_path;
    OCIO::GradingPrimary grading_primary{OCIO::GRADING_LIN};
};
using DynamicCDLMap = std::map<std::string, DynamicCDL>;

class ShaderBuilder {
  public:
    enum class DynamicCDLMode { None, Config, Processor };

    ShaderBuilder() = default;

    ShaderBuilder &setConfig(const OCIO::ConstConfigRcPtr &config);
    ShaderBuilder &setContext(const OCIO::ConstContextRcPtr &context);
    ShaderBuilder &setDynamicCDLs(const DynamicCDLMap &cdls);
    ShaderBuilder &setDynamicCDLMode(DynamicCDLMode mode);
    ShaderBuilder &setTransform(const OCIO::ConstTransformRcPtr &transform);
    ShaderBuilder &setLanguage(OCIO::GpuLanguage lang);
    ShaderBuilder &setFunctionName(const std::string &func);
    ShaderBuilder &setResourcePrefix(const std::string &prefix);

    std::string getShaderText();
    std::string getCacheString() const;
    void setupTextures(ColourOperationDataPtr &op_data) const;
    void updateUniforms(utility::JsonStore &uniforms, float exposure, float gamma);

  private:
    utility::JsonStore _metadata;
    OCIO::ConstConfigRcPtr _config;
    OCIO::ConstContextRcPtr _context;
    OCIO::ConstTransformRcPtr _transform;
    OCIO::GpuLanguage _lang;
    std::string _func_name;
    std::string _resource_prefix;

    OCIO::ConstConfigRcPtr getDynamicConfig();
    OCIO::ConstProcessorRcPtr getProcessor();
    OCIO::ConstGpuShaderDescRcPtr getShaderDesc();
    void updateDynamicProperties(float exposure, float gamma);
    void updateDynamicPropertiesUniforms(utility::JsonStore &uniforms);
    void updateDynamicCDLs(utility::JsonStore &uniforms);

    OCIO::ConstConfigRcPtr _dynamic_config;
    OCIO::ConstProcessorRcPtr _dynamic_processor;
    OCIO::GpuShaderDescRcPtr _dynamic_shader_desc;
    DynamicCDLMap _dynamic_cdls;
    DynamicCDLMode _dynamic_cdl_mode{DynamicCDLMode::None};
};

} // namespace xstudio::colour_pipeline::ocio