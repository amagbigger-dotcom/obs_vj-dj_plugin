#pragma once

#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include "effects/vj_config.hpp"

struct CustomShaderEntry {
    std::string id;          // e.g. "custom_0", "custom_1"
    std::string name;        // e.g. "Cyber Tunnel 3D"
    std::string category;    // "ISF / Raymarching"
    std::vector<VjParamDef> params; // Exactly 8 parameters
    std::vector<std::string> param_names; // Stored memory backing params[i].name
    std::vector<std::string> param_ids;   // Stored memory backing params[i].id
    std::string hlsl_function_name; // e.g. "render_custom_0"
    std::string hlsl_code;   // Full function: float4 render_custom_0(float2 uv) { ... }
    std::string raw_user_code; // Original code entered by user
};

class CustomShaderManager {
public:
    static CustomShaderManager &instance();

    void loadCustomShaders();
    void saveCustomShaders();

    const std::vector<CustomShaderEntry> &customShaders() const { return custom_shaders_; }

    // Live Direct3D 11 test compilation on GPU
    bool testCompileShader(const std::string &hlsl_function_code, std::string &out_error);

    // Parse AI clipboard/editor text (supports [NAME], [CATEGORY], [PARAMS], [CODE] or JSON or raw code)
    bool parseAiShaderText(const std::string &text,
                           std::string &out_name,
                           std::string &out_category,
                           std::vector<VjParamDef> &out_params,
                           std::vector<std::string> &out_param_names,
                           std::vector<std::string> &out_param_ids,
                           std::string &out_hlsl_code,
                           std::string &out_error);

    // Add or update custom shader
    bool addOrUpdateCustomShader(const std::string &id, // empty for new
                                 const std::string &name,
                                 const std::string &category,
                                 const std::vector<VjParamDef> &params,
                                 const std::vector<std::string> &param_names,
                                 const std::vector<std::string> &param_ids,
                                 const std::string &hlsl_code,
                                 const std::string &raw_user_code,
                                 std::string &out_id,
                                 std::string &out_error);

    // Delete custom shader by ID
    bool deleteCustomShader(const std::string &id);

    // Returns combined definitions (15 built-in + all custom shaders)
    const std::vector<VjShaderDef> &getAllShaderDefinitions();

    // Generates HLSL function declarations and dispatch statements
    std::string getCustomShadersHlslDeclarations() const;
    std::string getCustomShadersDispatchHlsl() const;

    // Version counter to trigger GPU reloads
    uint32_t version() const { return version_.load(std::memory_order_relaxed); }
    void incrementVersion() { version_.fetch_add(1, std::memory_order_relaxed); }

    // Pre-crafted clipboard prompt for external AI
    static const QString &getAiPromptTemplate();

private:
    CustomShaderManager();
    ~CustomShaderManager() = default;
    CustomShaderManager(const CustomShaderManager &) = delete;
    CustomShaderManager &operator=(const CustomShaderManager &) = delete;

    void rebuildCombinedDefinitions();
    QString getConfigFilePath() const;

    std::vector<CustomShaderEntry> custom_shaders_;
    std::vector<VjShaderDef> combined_defs_;
    std::atomic<uint32_t> version_{1};
    bool is_loaded_ = false;
};
