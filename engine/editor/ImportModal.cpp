#include "ImportModal.h"
#include "render/OBJLoader.h"
#include "render/GLTFLoader.h"
#include "scene/LightSource.h"
#include "scene/Part.h"
#include "scene/LogicScript.h"
#include "ui/Frame.h"
#include "imgui.h"

#define ICON_CUBOID     "\ue524"
#define ICON_SUN        "\ue178"
#define ICON_BOX        "\ue061"
#define ICON_LAYOUT     "\ue12c"
#define ICON_FILE_CODE  "\ue0c3"
#define ICON_IMPORT     "\ue22f"
#define ICON_X          "\ue084"

void DrawImportModal(bool& showImportModal,
                     std::vector<std::shared_ptr<GameObject>>& sceneObjects,
                     GpuPipeline& pipeline,
                     std::function<void(std::shared_ptr<GameObject>)> addObject,
                     GameObject*& selectedObject) {
    if (showImportModal) {
        ImGui::OpenPopup("Import Object");
    }
    if (ImGui::BeginPopupModal("Import Object", &showImportModal, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Spacing();

        if (ImGui::TreeNodeEx(ICON_CUBOID " 3D Objects", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::MenuItem(ICON_CUBOID "  3D Cube (GameObject)")) {
                auto newObj = std::make_shared<GameObject>("3D Cube", Vector3D(0, 0, 0), Vector3D(0, 0, 0), Vector3D(1, 1, 1));
                addObject(newObj);
                showImportModal = false; ImGui::CloseCurrentPopup();
            }
            if (ImGui::MenuItem(ICON_SUN "  LightSource")) {
                auto newObj = std::make_shared<LightSource>("LightSource");
                addObject(newObj);
                showImportModal = false; ImGui::CloseCurrentPopup();
            }
            if (ImGui::MenuItem(ICON_BOX "  Part")) {
                auto newObj = std::make_shared<Part>("Part");
                addObject(newObj);
                showImportModal = false; ImGui::CloseCurrentPopup();
            }
            ImGui::TreePop();
            ImGui::Spacing();
        }

        if (ImGui::TreeNodeEx(ICON_LAYOUT " 2D UI Elements", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::MenuItem(ICON_LAYOUT "  2D Frame")) {
                auto newObj = std::make_shared<Frame>("Frame", Position2D(0, 0, 100, 100), Size2D(0, 0, 200, 150));
                addObject(newObj);
                showImportModal = false; ImGui::CloseCurrentPopup();
            }
            if (ImGui::MenuItem(ICON_LAYOUT "  2D Panel (GameObject)")) {
                auto newObj = std::make_shared<GameObject>("Panel", Position2D(0, 0, 150, 150), Size2D(0, 0, 300, 200));
                addObject(newObj);
                showImportModal = false; ImGui::CloseCurrentPopup();
            }
            ImGui::TreePop();
            ImGui::Spacing();
        }

        if (ImGui::TreeNodeEx(ICON_FILE_CODE " Scripts & Logic", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::MenuItem(ICON_FILE_CODE "  Logic Script")) {
                auto newObj = std::make_shared<LogicScript>("LogicScript");
                addObject(newObj);
                selectedObject = newObj.get();
                showImportModal = false; ImGui::CloseCurrentPopup();
            }
            ImGui::TreePop();
            ImGui::Spacing();
        }

        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextDisabled(ICON_IMPORT " Import External Models");

        static float importScale = 1.0f;
        static bool importFlipWinding = false;

        ImGui::SetNextItemWidth(250);
        ImGui::DragFloat("Uniform Scale", &importScale, 0.01f, 0.01f, 100.0f, "%.2f");
        ImGui::SameLine();
        ImGui::Checkbox("Flip Winding", &importFlipWinding);

        static char objPathBuf[256] = "assets/suzanne.obj";
        ImGui::PushItemWidth(250);
        ImGui::InputText("##objpath", objPathBuf, sizeof(objPathBuf));
        ImGui::PopItemWidth();
        if (ImGui::MenuItem(ICON_IMPORT "  Import wavefront .obj")) {
            std::vector<Vertex> vertices;
            std::vector<uint32_t> indices;
            Vector3D center(0, 0, 0);
            if (LoadObj(objPathBuf, vertices, indices, center)) {
                if (importFlipWinding) {
                    for (size_t i = 0; i + 2 < indices.size(); i += 3)
                        std::swap(indices[i + 1], indices[i + 2]);
                }
                SDL_GPUBuffer* rawVB = pipeline.CreateAndUploadBuffer(vertices.data(), (Uint32)(vertices.size() * sizeof(Vertex)), SDL_GPU_BUFFERUSAGE_VERTEX);
                SDL_GPUBuffer* rawIB = pipeline.CreateAndUploadBuffer(indices.data(), (Uint32)(indices.size() * sizeof(uint32_t)), SDL_GPU_BUFFERUSAGE_INDEX);
                if (rawVB && rawIB) {
                    auto newObj = std::make_shared<GameObject>("Imported OBJ", center, Vector3D(0, 0, 0), Vector3D(importScale, importScale, importScale));
                    newObj->CustomVertexBuffer = std::shared_ptr<SDL_GPUBuffer>(rawVB, GpuBufferDeleter{pipeline.Device});
                    newObj->CustomIndexBuffer = std::shared_ptr<SDL_GPUBuffer>(rawIB, GpuBufferDeleter{pipeline.Device});
                    newObj->CustomIndexCount = (uint32_t)indices.size();
                    newObj->GpuDevice = pipeline.Device;
                    addObject(newObj);
                    showImportModal = false; ImGui::CloseCurrentPopup();
                }
            }
        }

        static char gltfPathBuf[256] = "scene.gltf";
        ImGui::PushItemWidth(250);
        ImGui::InputText("##gltfpath", gltfPathBuf, sizeof(gltfPathBuf));
        ImGui::PopItemWidth();
        if (ImGui::MenuItem(ICON_IMPORT "  Import glTF/GLB")) {
            std::vector<Vertex> vertices;
            std::vector<uint32_t> indices;
            Vector3D center(0, 0, 0);
            SDL_GPUTexture* importedTexture = nullptr;
            if (LoadGLTF(gltfPathBuf, vertices, indices, center, pipeline.Device, &importedTexture)) {
                if (importFlipWinding) {
                    for (size_t i = 0; i + 2 < indices.size(); i += 3)
                        std::swap(indices[i + 1], indices[i + 2]);
                }
                SDL_GPUBuffer* rawVB = pipeline.CreateAndUploadBuffer(vertices.data(), (Uint32)(vertices.size() * sizeof(Vertex)), SDL_GPU_BUFFERUSAGE_VERTEX);
                SDL_GPUBuffer* rawIB = pipeline.CreateAndUploadBuffer(indices.data(), (Uint32)(indices.size() * sizeof(uint32_t)), SDL_GPU_BUFFERUSAGE_INDEX);
                if (rawVB && rawIB) {
                    auto newObj = std::make_shared<GameObject>("Imported glTF", center, Vector3D(0, 0, 0), Vector3D(importScale, importScale, importScale));
                    newObj->CustomVertexBuffer = std::shared_ptr<SDL_GPUBuffer>(rawVB, GpuBufferDeleter{pipeline.Device});
                    newObj->CustomIndexBuffer = std::shared_ptr<SDL_GPUBuffer>(rawIB, GpuBufferDeleter{pipeline.Device});
                    if (importedTexture) {
                        newObj->CustomTexture = std::shared_ptr<SDL_GPUTexture>(importedTexture, GpuTextureDeleter{pipeline.Device});
                    }
                    newObj->CustomIndexCount = (uint32_t)indices.size();
                    newObj->GpuDevice = pipeline.Device;
                    addObject(newObj);
                    showImportModal = false; ImGui::CloseCurrentPopup();
                } else if (importedTexture) {
                    SDL_ReleaseGPUTexture(pipeline.Device, importedTexture);
                }
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        if (ImGui::MenuItem(ICON_X "  Cancel")) {
            showImportModal = false; ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}
