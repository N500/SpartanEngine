/*
Copyright(c) 2016-2021 Panos Karabelas

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
copies of the Software, and to permit persons to whom the Software is furnished
to do so, subject to the following conditions :

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

//= INCLUDES =======================
#include "Widget_ShaderEditor.h"
#include "Input/Input.h"
#include "Rendering/Renderer.h"
#include "Rendering/ShaderLight.h"
#include "Rendering/ShaderGBuffer.h"
#include <fstream>
#include "../ImGui_Extension.h"
//==================================

//= NAMESPACES =========
using namespace std;
using namespace Spartan;
//======================

Widget_ShaderEditor::Widget_ShaderEditor(Editor* editor) : Widget(editor)
{
    m_title         = "Shader Editor";
    m_flags         |= ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoScrollbar;
    m_is_visible    = false;
    m_size          = ImVec2(1366, 1000);
    m_text_editor   = make_unique<Widget_TextEditor>();
    m_renderer      = m_context->GetSubsystem<Renderer>();
    m_input         = m_context->GetSubsystem<Input>();
    m_position      = k_widget_position_screen_center;
}

void Widget_ShaderEditor::TickVisible()
{
    ShowShaderSource();
    ImGui::SameLine();
    ShowShaderList();
}

void Widget_ShaderEditor::ShowShaderSource()
{
    ImGui::BeginGroup();
    {
        ImGui::Text(m_shader ? m_shader_name.c_str() : "Select a shader");

        if (m_shader)
        {
            if (ImGui::BeginChild("##shader_source", ImVec2(m_size.x * 0.7f, 0.0f)))
            {
                // Shader source
                if (ImGui::BeginTabBar("#shader_tab_bar", ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_FittingPolicyResizeDown))
                {
                    const std::vector<std::string>& names      = m_shader->GetNames();
                    const std::vector<std::string>& sources    = m_shader->GetSources();

                    for (uint32_t i = 0; i < static_cast<uint32_t>(names.size()); i++)
                    {
                        if (ImGui::BeginTabItem(names[i].c_str()))
                        {
                            // Set text
                            if (m_index_displayed != i)
                            {
                                m_text_editor->SetText(sources[i]);
                                m_index_displayed = i;
                            }

                            // Handle keyboard shortcuts
                            if (m_input->GetKeyDown(KeyCode::Ctrl_Left) && m_input->GetKeyDown(KeyCode::C))
                            {
                                m_text_editor->Copy();
                            }

                            if (m_input->GetKeyDown(KeyCode::Ctrl_Left) && m_input->GetKeyDown(KeyCode::X))
                            {
                                m_text_editor->Cut();
                            }

                            if (m_input->GetKeyDown(KeyCode::Ctrl_Left) && m_input->GetKeyDown(KeyCode::V))
                            {
                                m_text_editor->Paste();
                            }

                            if (m_input->GetKeyDown(KeyCode::Ctrl_Left) && m_input->GetKeyDown(KeyCode::Z))
                            {
                                m_text_editor->Undo();
                            }

                            if (m_input->GetKeyDown(KeyCode::Ctrl_Left) && m_input->GetKeyDown(KeyCode::Y))
                            {
                                m_text_editor->Redo();
                            }

                            // Render
                            m_text_editor->Render("Title", ImVec2(0.0f, ImGui::GetContentRegionMax().y - 60.0f)); // shrink y to bring the compile button into view

                            // Update shader
                            if (m_text_editor->IsTextChanged())
                            {
                                m_shader->SetSource(i, m_text_editor->GetText());
                            }

                            ImGui::EndTabItem();
                        }
                    }
                    ImGui::EndTabBar();
                }

                if (ImGuiEx::Button("Compile"))
                {
                    if (m_index_displayed != -1)
                    {
                        const std::vector<std::string>& file_paths  = m_shader->GetFilePaths();
                        const std::vector<std::string>& sources     = m_shader->GetSources();

                        // Save all files
                        for (uint32_t i = 0; i < static_cast<uint32_t>(file_paths.size()); i++)
                        {
                            ofstream out(file_paths[i]);
                            out << sources[i];
                            out.flush();
                            out.close();
                        }

                        // Compile synchronously to make it obvious when the first rendered frame (with your changes) shows up
                        bool async = false;
                        m_shader->Compile(m_shader->GetShaderStage(), m_shader->GetFilePath(), async);
                    }
                }

                ImGui::EndChild();
            }
        }
    }
    ImGui::EndGroup();
}

void Widget_ShaderEditor::ShowShaderList()
{
    GetShaderInstances();

    ImGui::BeginGroup();
    {
        ImGui::Text("Shaders");

        if (ImGui::BeginChild("##shader_list", ImVec2(0.0f, 0.0f), true, ImGuiWindowFlags_HorizontalScrollbar))
        {
            for (RHI_Shader* shader : m_shaders)
            {
                // Get name
                string name = shader->GetObjectName();

                // Append stage
                if (shader->GetShaderStage() == RHI_Shader_Vertex)
                {
                    name += "_Vertex";
                }
                else if (shader->GetShaderStage() == RHI_Shader_Pixel)
                {
                    name += "_Pixel";
                }
                else if (shader->GetShaderStage() == RHI_Shader_Compute)
                {
                    name += "_Compute";
                }
                else
                {
                    name += "_Unknown";
                }

                // Append defines
                for (const auto& define : shader->GetDefines())
                {
                    if (define.second != "0")
                    {
                        name += "_" + define.first;
                    }
                }

                if (ImGuiEx::Button(name.c_str()) || m_first_run)
                {
                    m_shader            = shader;
                    m_shader_name       = name;
                    m_index_displayed   = -1;
                    m_first_run         = false;

                    // Reload in case it has been modified
                    m_shader->LoadSource(m_shader->GetFilePath());
                }
            }
            ImGui::EndChild();
        }
    }
    ImGui::EndGroup();
}

void Widget_ShaderEditor::GetShaderInstances()
{
    unordered_map<RendererShader, shared_ptr<RHI_Shader>> shaders = m_renderer->GetShaders();
    m_shaders.clear();

    for (const auto& it : shaders)
    {
        if (it.second->IsCompiled())
        {
            m_shaders.emplace_back(it.second.get());
        }
    }

    // Gbuffer - Contained in shaders but not compiled itself, it holds variations in it instead
    for (const auto& it : ShaderGBuffer::GetVariations())
    {
        m_shaders.emplace_back(it.second.get());
    }

    // Light - Contained in shaders but not compiled itself, it holds variations in it instead
    for (const auto& it : ShaderLight::GetVariations())
    {
        m_shaders.emplace_back(it.second.get());
    }

    // Order them alphabetically
    sort(m_shaders.begin(), m_shaders.end(), [](RHI_Shader* a, RHI_Shader* b) { return a->GetObjectName() < b->GetObjectName(); });
}
