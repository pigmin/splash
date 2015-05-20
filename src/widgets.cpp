#include "widgets.h"

#include <array>
#include <fstream>
#include <imgui.h>

#include "camera.h"
#include "log.h"
#include "object.h"
#include "scene.h"
#include "texture.h"
#include "texture_image.h"
#include "timer.h"

#if HAVE_GPHOTO
#include "colorcalibrator.h"
#endif

using namespace std;

namespace Splash
{

/*************/
GuiWidget::GuiWidget(string name)
{
    _name = name;
}

/*************/
GuiTextBox::GuiTextBox(string name)
    : GuiWidget(name)
{
}

/*************/
void GuiTextBox::render()
{
    if (getText)
    {
        if (ImGui::CollapsingHeader(_name.c_str()))
            ImGui::Text(getText().c_str());
    }
}

/*************/
void GuiControl::render()
{
    if (ImGui::CollapsingHeader(_name.c_str()))
    {
        // World control
        ImGui::Text("World configuration (not saved!)");
        static auto worldFramerate = 60;
        if (ImGui::InputInt("World framerate", &worldFramerate, 1, 100, ImGuiInputTextFlags_EnterReturnsTrue))
        {
            worldFramerate = std::max(worldFramerate, 0);
            auto scene = _scene.lock();
            scene->sendMessageToWorld("framerate", {worldFramerate});
        }
        static auto syncTestFrameDelay = 0;
        if (ImGui::InputInt("Frames between color swap", &syncTestFrameDelay, 1, 100, ImGuiInputTextFlags_EnterReturnsTrue))
        {
            syncTestFrameDelay = std::max(syncTestFrameDelay, 0);
            auto scene = _scene.lock();
            scene->sendMessageToWorld("swapTest", {syncTestFrameDelay});
        }
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Node view
        if (!_nodeView)
        {
            auto nodeView = make_shared<GuiNodeView>("Nodes");
            nodeView->setScene(_scene);
            _nodeView = dynamic_pointer_cast<GuiWidget>(nodeView);
        }
        ImGui::Text("Configuration global view");
        _nodeView->render();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Node configuration
        ImGui::Text("Objects configuration (saved!)");
        // Select the object the control
        {
            vector<string> objectNames = getObjectNames();
            vector<const char*> items;
            for (auto& name : objectNames)
                items.push_back(name.c_str());
            ImGui::Combo("Selected object", &_targetIndex, items.data(), items.size());
        }

        // Initialize the target
        if (_targetIndex >= 0)
        {
            vector<string> objectNames = getObjectNames();
            if (objectNames.size() <= _targetIndex)
                return;
            _targetObjectName = objectNames[_targetIndex];
        }

        if (_targetObjectName == "")
            return;

        auto scene = _scene.lock();

        bool isDistant = false;
        if (scene->_ghostObjects.find(_targetObjectName) != scene->_ghostObjects.end())
            isDistant = true;

        unordered_map<string, Values> attributes;
        if (!isDistant)
            attributes = scene->_objects[_targetObjectName]->getAttributes();
        else
            attributes = scene->_ghostObjects[_targetObjectName]->getAttributes();

        for (auto& attr : attributes)
        {
            if (attr.second.size() > 4 || attr.second.size() == 0)
                continue;

            if (attr.second[0].getType() == Value::Type::i
                || attr.second[0].getType() == Value::Type::f)
            {
                int precision = 0;
                if (attr.second[0].getType() == Value::Type::f)
                    precision = 2;

                if (attr.second.size() == 1)
                {
                    float tmp = attr.second[0].asFloat();
                    float step = attr.second[0].getType() == Value::Type::f ? 0.01 * tmp : 1.f;
                    if (ImGui::InputFloat(attr.first.c_str(), &tmp, step, step, precision, ImGuiInputTextFlags_EnterReturnsTrue))
                    {
                        if (!isDistant)
                            scene->_objects[_targetObjectName]->setAttribute(attr.first, {tmp});
                        else
                        {
                            scene->_ghostObjects[_targetObjectName]->setAttribute(attr.first, {tmp});
                            scene->sendMessageToWorld("sendAll", {_targetObjectName, attr.first, tmp});
                        }
                    }
                }
                else if (attr.second.size() == 2)
                {
                    vector<float> tmp;
                    tmp.push_back(attr.second[0].asFloat());
                    tmp.push_back(attr.second[1].asFloat());
                    if (ImGui::InputFloat2(attr.first.c_str(), tmp.data(), precision, ImGuiInputTextFlags_EnterReturnsTrue))
                    {
                        if (!isDistant)
                            scene->_objects[_targetObjectName]->setAttribute(attr.first, {tmp[0], tmp[1]});
                        else
                        {
                            scene->_ghostObjects[_targetObjectName]->setAttribute(attr.first, {tmp[0], tmp[1]});
                            scene->sendMessageToWorld("sendAll", {_targetObjectName, attr.first, tmp[0], tmp[1]});
                        }
                    }
                }
                else if (attr.second.size() == 3)
                {
                    vector<float> tmp;
                    tmp.push_back(attr.second[0].asFloat());
                    tmp.push_back(attr.second[1].asFloat());
                    tmp.push_back(attr.second[2].asFloat());
                    if (ImGui::InputFloat3(attr.first.c_str(), tmp.data(), precision, ImGuiInputTextFlags_EnterReturnsTrue))
                    {
                        if (!isDistant)
                            scene->_objects[_targetObjectName]->setAttribute(attr.first, {tmp[0], tmp[1], tmp[2]});
                        else
                        {
                            scene->_ghostObjects[_targetObjectName]->setAttribute(attr.first, {tmp[0], tmp[1], tmp[2]});
                            scene->sendMessageToWorld("sendAll", {_targetObjectName, attr.first, tmp[0], tmp[1], tmp[2]});
                        }
                    }
                }
                else if (attr.second.size() == 4)
                {
                    vector<float> tmp;
                    tmp.push_back(attr.second[0].asFloat());
                    tmp.push_back(attr.second[1].asFloat());
                    tmp.push_back(attr.second[2].asFloat());
                    tmp.push_back(attr.second[3].asFloat());
                    if (ImGui::InputFloat4(attr.first.c_str(), tmp.data(), precision, ImGuiInputTextFlags_EnterReturnsTrue))
                    {
                        if (!isDistant)
                            scene->_objects[_targetObjectName]->setAttribute(attr.first, {tmp[0], tmp[1], tmp[2], tmp[3]});
                        else
                        {
                            scene->_ghostObjects[_targetObjectName]->setAttribute(attr.first, {tmp[0], tmp[1], tmp[2], tmp[3]});
                            scene->sendMessageToWorld("sendAll", {_targetObjectName, attr.first, tmp[0], tmp[1], tmp[2], tmp[3]});
                        }
                    }
                }
            }
            else if (attr.second.size() == 1 && attr.second[0].getType() == Value::Type::v)
            {
                // We skip anything that looks like a vector / matrix
                // (for usefulness reasons...)
                Values values = attr.second[0].asValues();
                if (values.size() > 16)
                {
                    if (values[0].getType() == Value::Type::i || values[0].getType() == Value::Type::f)
                    {
                        float minValue = numeric_limits<float>::max();
                        float maxValue = numeric_limits<float>::min();
                        vector<float> samples;
                        for (auto& v : values)
                        {
                            float value = v.asFloat();
                            maxValue = std::max(value, maxValue);
                            minValue = std::min(value, minValue);
                            samples.push_back(value);
                        }
                        
                        ImGui::PlotLines(attr.first.c_str(), samples.data(), samples.size(), samples.size(), ("[" + to_string(minValue) + ", " + to_string(maxValue) + "]").c_str(), minValue, maxValue, ImVec2(0, 100));
                    }
                }
            }
            else if (attr.second[0].getType() == Value::Type::s)
            {
                for (auto& v : attr.second)
                {
                    string tmp = v.asString();
                    ImGui::Text(tmp.c_str());
                }
            }
        }
    }
}

/*************/
vector<string> GuiControl::getObjectNames()
{
    auto scene = _scene.lock();
    vector<string> objNames;

    for (auto& o : scene->_objects)
    {
        if (!o.second->_savable)
            continue;
        objNames.push_back(o.first);
    }
    for (auto& o : scene->_ghostObjects)
    {
        if (!o.second->_savable)
            continue;
        objNames.push_back(o.first);
    }

    return objNames;
}

/*************/
int GuiControl::updateWindowFlags()
{
    ImGuiWindowFlags flags = 0;
    if (_nodeView)
        flags |= _nodeView->updateWindowFlags();
    return flags;
}

/*************/
GuiGlobalView::GuiGlobalView(string name)
    : GuiWidget(name)
{
}

/*************/
void GuiGlobalView::render()
{
    if (ImGui::CollapsingHeader(_name.c_str()))
    {
        if (_camera != nullptr)
        {
            if (_camera == _guiCamera)
                _guiCamera->setAttribute("size", {ImGui::GetWindowWidth(), ImGui::GetWindowWidth() * 3 / 4});

            _camera->render();

            Values size;
            _camera->getAttribute("size", size);

            double leftMargin = ImGui::GetCursorScreenPos().x - ImGui::GetWindowPos().x;
            ImVec2 winSize = ImGui::GetWindowSize();
            int w = std::max(400.0, winSize.x - 4 * leftMargin);
            int h = w * size[1].asInt() / size[0].asInt();

            _camWidth = w;
            _camHeight = h;

            if (ImGui::Button("Next camera"))
                nextCamera();
            ImGui::SameLine();
            if (ImGui::Button("Hide other cameras"))
                switchHideOtherCameras();
            ImGui::SameLine();
            if (ImGui::Button("Show all points"))
                showAllCalibrationPoints();
            ImGui::SameLine();
            if (ImGui::Button("Calibrate camera"))
                doCalibration();

            ImGui::Text(("Current camera: " + _camera->getName()).c_str());

            ImGui::Image((void*)(intptr_t)_camera->getTextures()[0]->getTexId(), ImVec2(w, h), ImVec2(0, 1), ImVec2(1, 0));
            if (ImGui::IsItemHovered())
            {
                _noMove = true;
                processKeyEvents();
                processMouseEvents();
            }
            else
                _noMove = false;
        }
    }
}

/*************/
int GuiGlobalView::updateWindowFlags()
{
    ImGuiWindowFlags flags = 0;
    if (_noMove)
    {
        flags |= ImGuiWindowFlags_NoMove;
        flags |= ImGuiWindowFlags_NoScrollWithMouse;
    }
    return flags;
}

/*************/
void GuiGlobalView::setCamera(CameraPtr cam)
{
    if (cam != nullptr)
    {
        _camera = cam;
        _guiCamera = cam;
        _camera->setAttribute("size", {800, 600});
    }
}

/*************/
void GuiGlobalView::setObject(ObjectPtr obj)
{
    _camera->linkTo(obj);
}

/*************/
void GuiGlobalView::nextCamera()
{
    auto scene = _scene.lock();
    vector<CameraPtr> cameras;
    for (auto& obj : scene->_objects)
        if (dynamic_pointer_cast<Camera>(obj.second).get() != nullptr)
            cameras.push_back(dynamic_pointer_cast<Camera>(obj.second));
    for (auto& obj : scene->_ghostObjects)
        if (dynamic_pointer_cast<Camera>(obj.second).get() != nullptr)
            cameras.push_back(dynamic_pointer_cast<Camera>(obj.second));

    // Empty previous camera parameters
    _previousCameraParameters.clear();

    // Ensure that all cameras are shown
    _camerasHidden = false;
    for (auto& cam : cameras)
        scene->sendMessageToWorld("sendAll", {cam->getName(), "hide", 0});

    scene->sendMessageToWorld("sendAll", {_camera->getName(), "frame", 0});
    scene->sendMessageToWorld("sendAll", {_camera->getName(), "displayCalibration", 0});

    if (cameras.size() == 0)
        _camera = _guiCamera;
    else if (_camera == _guiCamera)
        _camera = cameras[0];
    else
    {
        for (int i = 0; i < cameras.size(); ++i)
        {
            if (cameras[i] == _camera && i == cameras.size() - 1)
            {
                _camera = _guiCamera;
                break;
            }
            else if (cameras[i] == _camera)
            {
                _camera = cameras[i + 1];
                break;
            }
        }
    }

    if (_camera != _guiCamera)
    {
        scene->sendMessageToWorld("sendAll", {_camera->getName(), "frame", 1});
        scene->sendMessageToWorld("sendAll", {_camera->getName(), "displayCalibration", 1});
    }

    return;
}

/*************/
void GuiGlobalView::showAllCalibrationPoints()
{
    auto scene = _scene.lock();
    scene->sendMessageToWorld("sendAll", {_camera->getName(), "switchShowAllCalibrationPoints"});
}

/*************/
void GuiGlobalView::doCalibration()
{
    CameraParameters params;
     // We keep the current values
    _camera->getAttribute("eye", params.eye);
    _camera->getAttribute("target", params.target);
    _camera->getAttribute("up", params.up);
    _camera->getAttribute("fov", params.fov);
    _camera->getAttribute("principalPoint", params.principalPoint);
    _previousCameraParameters.push_back(params);

    // Calibration
    _camera->doCalibration();
    propagateCalibration();

    return;
}

/*************/
void GuiGlobalView::propagateCalibration()
{
    bool isDistant {false};
    auto scene = _scene.lock();
    for (auto& obj : scene->_ghostObjects)
        if (_camera->getName() == obj.second->getName())
            isDistant = true;
    
    if (isDistant)
    {
        vector<string> properties {"eye", "target", "up", "fov", "principalPoint"};
        auto scene = _scene.lock();
        for (auto& p : properties)
        {
            Values values;
            _camera->getAttribute(p, values);

            Values sendValues {_camera->getName(), p};
            for (auto& v : values)
                sendValues.push_back(v);

            scene->sendMessageToWorld("sendAll", sendValues);
        }
    }
}

/*************/
void GuiGlobalView::switchHideOtherCameras()
{
    auto scene = _scene.lock();
    vector<CameraPtr> cameras;
    for (auto& obj : scene->_objects)
        if (dynamic_pointer_cast<Camera>(obj.second).get() != nullptr)
            cameras.push_back(dynamic_pointer_cast<Camera>(obj.second));
    for (auto& obj : scene->_ghostObjects)
        if (dynamic_pointer_cast<Camera>(obj.second).get() != nullptr)
            cameras.push_back(dynamic_pointer_cast<Camera>(obj.second));

    if (!_camerasHidden)
    {
        for (auto& cam : cameras)
            if (cam.get() != _camera.get())
                scene->sendMessageToWorld("sendAll", {cam->getName(), "hide", 1});
        _camerasHidden = true;
    }
    else
    {
        for (auto& cam : cameras)
            if (cam.get() != _camera.get())
                scene->sendMessageToWorld("sendAll", {cam->getName(), "hide", 0});
        _camerasHidden = false;
    }
}

/*************/
void GuiGlobalView::processKeyEvents()
{
    ImGuiIO& io = ImGui::GetIO();

    if (io.KeysDown[' '] && io.KeysDownTime[' '] == 0.0)
    {
        nextCamera();
        return;
    }
    else if (io.KeysDown['A'] && io.KeysDownTime['A'] == 0.0)
    {
        showAllCalibrationPoints();
        return;
    }
    else if (io.KeysDown['C'] && io.KeysDownTime['C'] == 0.0)
    {
        doCalibration();
        return;
    }
    else if (io.KeysDown['H'] && io.KeysDownTime['H'] == 0.0)
    {
        switchHideOtherCameras();
        return;
    }
    // Reset to the previous camera calibration
    else if (io.KeysDown['R'] && io.KeysDownTime['R'] == 0.0)
    {
        if (_previousCameraParameters.size() == 0)
            return;

        Log::get() << Log::MESSAGE << "GuiGlobalView::" << __FUNCTION__ << " - Reverting camera to previous parameters" << Log::endl;

        auto params = _previousCameraParameters.back();
        _previousCameraParameters.pop_back();

        _camera->setAttribute("eye", params.eye);
        _camera->setAttribute("target", params.target);
        _camera->setAttribute("up", params.up);
        _camera->setAttribute("fov", params.fov);
        _camera->setAttribute("principalPoint", params.principalPoint);

        auto scene = _scene.lock();
        for (auto& obj : scene->_ghostObjects)
            if (_camera->getName() == obj.second->getName())
            {
                scene->sendMessageToWorld("sendAll", {_camera->getName(), "eye", params.eye[0], params.eye[1], params.eye[2]});
                scene->sendMessageToWorld("sendAll", {_camera->getName(), "target", params.target[0], params.target[1], params.target[2]});
                scene->sendMessageToWorld("sendAll", {_camera->getName(), "up", params.up[0], params.up[1], params.up[2]});
                scene->sendMessageToWorld("sendAll", {_camera->getName(), "fov", params.fov[0]});
                scene->sendMessageToWorld("sendAll", {_camera->getName(), "principalPoint", params.principalPoint[0], params.principalPoint[1]});
            }
    }
    // Arrow keys
    else
    {
        auto scene = _scene.lock();

        float delta = 1.f;
        if (io.KeyShift)
            delta = 0.1f;
        else if (io.KeyCtrl)
            delta = 10.f;
            
        if (io.KeysDown[262])
        {
            scene->sendMessageToWorld("sendAll", {_camera->getName(), "moveCalibrationPoint", delta, 0});
            _camera->moveCalibrationPoint(0.0, 0.0);
            propagateCalibration();
        }
        if (io.KeysDown[263])
        {
            scene->sendMessageToWorld("sendAll", {_camera->getName(), "moveCalibrationPoint", -delta, 0});
            _camera->moveCalibrationPoint(0.0, 0.0);
            propagateCalibration();
        }
        if (io.KeysDown[264])
        {
            scene->sendMessageToWorld("sendAll", {_camera->getName(), "moveCalibrationPoint", 0, -delta});
            _camera->moveCalibrationPoint(0.0, 0.0);
            propagateCalibration();
        }
        if (io.KeysDown[265])
        {
            scene->sendMessageToWorld("sendAll", {_camera->getName(), "moveCalibrationPoint", 0, delta});
            _camera->moveCalibrationPoint(0.0, 0.0);
            propagateCalibration();
        }

        return;
    }
}

/*************/
void GuiGlobalView::processMouseEvents()
{
    ImGuiIO& io = ImGui::GetIO();

    // Get mouse pos
    ImVec2 mousePos = ImVec2((io.MousePos.x - ImGui::GetCursorScreenPos().x) / _camWidth,
                             -(io.MousePos.y - ImGui::GetCursorScreenPos().y) / _camHeight);

    if (io.MouseDown[0])
    {
        // If selected camera is guiCamera, do nothing
        if (_camera == _guiCamera)
            return;

        // Set a calibration point
        auto scene = _scene.lock();
        if (io.KeyCtrl && io.MouseClicked[0])
        {
            auto scene = _scene.lock();
            Values position = _camera->pickCalibrationPoint(mousePos.x, mousePos.y);
            if (position.size() == 3)
                scene->sendMessageToWorld("sendAll", {_camera->getName(), "removeCalibrationPoint", position[0], position[1], position[2]});
        }
        else if (io.KeyShift) // Define the screenpoint corresponding to the selected calibration point
            scene->sendMessageToWorld("sendAll", {_camera->getName(), "setCalibrationPoint", mousePos.x * 2.f - 1.f, mousePos.y * 2.f - 1.f});
        else if (io.MouseClicked[0]) // Add a new calibration point
        {
            Values position = _camera->pickVertexOrCalibrationPoint(mousePos.x, mousePos.y);
            if (position.size() == 3)
            {
                scene->sendMessageToWorld("sendAll", {_camera->getName(), "addCalibrationPoint", position[0], position[1], position[2]});
                _previousPointAdded = position;
            }
            else
                scene->sendMessageToWorld("sendAll", {_camera->getName(), "deselectCalibrationPoint"});
        }
        return;
    }
    if (io.MouseClicked[1])
    {
        _newTarget = _camera->pickFragment(mousePos.x, mousePos.y);
    }
    if (io.MouseDownTime[1] > 0.0) 
    {
        // Move the camera
        if (!io.KeyCtrl && !io.KeyShift)
        {
            float dx = io.MouseDelta.x;
            float dy = io.MouseDelta.y;
            auto scene = _scene.lock();

            if (_camera != _guiCamera)
            {
                if (_newTarget.size() == 3)
                    scene->sendMessageToWorld("sendAll", {_camera->getName(), "rotateAroundPoint", dx / 100.f, dy / 100.f, 0, _newTarget[0].asFloat(), _newTarget[1].asFloat(), _newTarget[2].asFloat()});
                else
                    scene->sendMessageToWorld("sendAll", {_camera->getName(), "rotateAroundTarget", dx / 100.f, dy / 100.f, 0});
            }
            else
            {
                if (_newTarget.size() == 3)
                    _camera->setAttribute("rotateAroundPoint", {dx / 100.f, dy / 100.f, 0, _newTarget[0].asFloat(), _newTarget[1].asFloat(), _newTarget[2].asFloat()});
                else
                    _camera->setAttribute("rotateAroundTarget", {dx / 100.f, dy / 100.f, 0});
            }
        }
        // Move the target and the camera (in the camera plane)
        else if (io.KeyShift && !io.KeyCtrl)
        {
            float dx = io.MouseDelta.x;
            float dy = io.MouseDelta.y;
            auto scene = _scene.lock();
            if (_camera != _guiCamera)
                scene->sendMessageToWorld("sendAll", {_camera->getName(), "pan", -dx / 100.f, dy / 100.f, 0.f});
            else
                _camera->setAttribute("pan", {-dx / 100.f, dy / 100.f, 0});
        }
        else if (!io.KeyShift && io.KeyCtrl)
        {
            float dy = io.MouseDelta.y / 100.f;
            auto scene = _scene.lock();
            if (_camera != _guiCamera)
                scene->sendMessageToWorld("sendAll", {_camera->getName(), "forward", dy});
            else
                _camera->setAttribute("forward", {dy});
        }
    }
    if (io.MouseWheel != 0)
    {
        Values fov;
        _camera->getAttribute("fov", fov);
        float camFov = fov[0].asFloat();

        camFov += io.MouseWheel;
        camFov = std::max(2.f, std::min(180.f, camFov));

        auto scene = _scene.lock();
        if (_camera != _guiCamera)
            scene->sendMessageToWorld("sendAll", {_camera->getName(), "fov", camFov});
        else
            _camera->setAttribute("fov", {camFov});
    }
}

/*************/
void GuiGraph::render()
{
    if (ImGui::CollapsingHeader(_name.c_str()))
    {
        auto& durationMap = Timer::get().getDurationMap();

        for (auto& t : durationMap)
        {
            if (_durationGraph.find(t.first) == _durationGraph.end())
                _durationGraph[t.first] = deque<unsigned long long>({t.second});
            else
            {
                if (_durationGraph[t.first].size() == _maxHistoryLength)
                    _durationGraph[t.first].pop_front();
                _durationGraph[t.first].push_back(t.second);
            }
        }

        if (_durationGraph.size() == 0)
            return;

        auto width = ImGui::GetWindowSize().x;
        for (auto& duration : _durationGraph)
        {
            float maxValue {0.f};
            vector<float> values;
            for (auto& v : duration.second)
            {
                maxValue = std::max((float)v * 0.001f, maxValue);
                values.push_back((float)v * 0.001f);
            }

            maxValue = ceil(maxValue * 0.1f) * 10.f;

            ImGui::PlotLines("", values.data(), values.size(), values.size(), (duration.first + " - " + to_string((int)maxValue) + "ms").c_str(), 0.f, maxValue, ImVec2(width - 30, 80));
        }
    }
}

/*************/
void GuiTemplate::render()
{
    if (!_templatesLoaded)
    {
        loadTemplates();
        _templatesLoaded = true;
    }

    if (_textures.size() == 0)
        return;

    if (ImGui::CollapsingHeader(_name.c_str()))
    {
        bool firstTemplate = true;
        for (auto& name : _names)
        {
            if (!firstTemplate)
                ImGui::SameLine(0, 2);
            firstTemplate = false;

            if (ImGui::ImageButton((void*)(intptr_t)_textures[name]->getTexId(), ImVec2(128, 128)))
            {
                string configPath = string(DATADIR) + "templates/" + name + ".json";
#if HAVE_OSX
                if (!ifstream(configPath, ios::in | ios::binary))
                    configPath = "../Resources/templates/" + name + ".json";
#endif
                auto scene = _scene.lock();
                scene->sendMessageToWorld("loadConfig", {configPath});
            }

            if (ImGui::IsItemHovered())
                ImGui::SetTooltip(_descriptions[name].data());
        }
    }
}

/*************/
void GuiTemplate::loadTemplates()
{
    auto examples = vector<string>();
    auto descriptions = vector<string>();
    
    // Try to read the template file
    ifstream in(string(DATADIR) + "templates.txt", ios::in | ios::binary);
    if (in)
    {
        auto newTemplate = false;
        auto templateName = string();
        auto templateDescription = string();
        auto endTemplate = false;

        for (array<char, 256> line; in.getline(&line[0], 256);)
        {
            auto strLine = string(line.data());
            if (!newTemplate)
            {
                if (strLine == "{")
                {
                    newTemplate = true;
                    endTemplate = false;
                    templateName = "";
                    templateDescription = "";
                }
            }
            else
            {
                if (strLine == "}")
                    endTemplate = true;
                else if (templateName == "")
                    templateName = strLine;
                else
                    templateDescription = strLine;
            }

            if (endTemplate)
            {
                examples.push_back(templateName);
                descriptions.push_back(templateDescription);
                templateName = "";
                templateDescription = "";
                newTemplate = false;
            }
        }
    }
    else
    {
        Log::get() << Log::WARNING << "GuiTemplate::" << __FUNCTION__ << " - Could not load the templates file list in " << DATADIR << "templates.txt" << Log::endl;
        return;
    }

    _textures.clear();
    _descriptions.clear();

    for (unsigned int i = 0; i < examples.size(); ++i)
    {
        auto& example = examples[i];
        auto& description = descriptions[i];

        glGetError();
        auto image = make_shared<Image>();
        image->setName("template_" + example);
        if (!image->read(string(DATADIR) + "templates/" + example + ".png"))
        {
#if HAVE_OSX
            if (!image->read("../Resources/templates/" + example + ".png"))
#endif
            continue;
        }

        auto texture = make_shared<Texture_Image>();
        texture->linkTo(image);
        texture->update();
        texture->flushPbo();

        _names.push_back(example);
        _textures[example] = texture;
        _descriptions[example] = description;
    }
}

/*************/
map<string, vector<string>> GuiNodeView::getObjectLinks()
{
    auto scene = _scene.lock();

    auto links = map<string, vector<string>>();

    for (auto& o : scene->_objects)
    {
        if (!o.second->_savable)
            continue;
        links[o.first] = vector<string>();
        auto linkedObjects = o.second->getLinkedObjects();
        for (auto& link : linkedObjects)
            links[o.first].push_back(link->getName());
    }
    for (auto& o : scene->_ghostObjects)
    {
        if (!o.second->_savable)
            continue;
        links[o.first] = vector<string>();
        auto linkedObjects = o.second->getLinkedObjects();
        for (auto& link : linkedObjects)
            links[o.first].push_back(link->getName());
    }

    return links;
}

/*************/
map<string, string> GuiNodeView::getObjectTypes()
{
    auto scene = _scene.lock();

    auto types = map<string, string>();

    for (auto& o : scene->_objects)
    {
        if (!o.second->_savable)
            continue;
        types[o.first] = o.second->getType();
    }
    for (auto& o : scene->_ghostObjects)
    {
        if (!o.second->_savable)
            continue;
        types[o.first] = o.second->getType();
    }

    return types;
}

/*************/
void GuiNodeView::render()
{
    //if (ImGui::CollapsingHeader(_name.c_str()))
    if (true)
    {
        // This defines the default positions for various node types
        static auto defaultPositionByType = map<string, ImVec2>({{"default", {8, 8}},
                                                                 {"window", {8, 32}},
                                                                 {"camera", {32, 64}},
                                                                 {"object", {8, 96}},
                                                                 {"texture", {32, 128}},
                                                                 {"image", {8, 160}},
                                                                 {"mesh", {32, 192}}
                                                                });
        std::map<std::string, int> shiftByType;

        // Begin a subwindow to enclose nodes
        ImGui::BeginChild("NodeView", ImVec2(_viewSize[0], _viewSize[1]), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        // Get objects and their relations
        auto objectLinks = getObjectLinks();
        auto objectTypes = getObjectTypes();

        // Objects useful for drawing
        auto drawList = ImGui::GetWindowDrawList();
        auto canvasPos = ImGui::GetCursorScreenPos();
        // Apply view shift
        canvasPos.x += _viewShift[0];
        canvasPos.y += _viewShift[1];

        // Draw objects
        for (auto& object : objectTypes)
        {
            auto& name = object.first;
            auto& type = object.second;
            type = type.substr(0, type.find("_"));

            // We keep count of the number of objects by type, more precisely their shifting
            auto shiftIt = shiftByType.find(type);
            if (shiftIt == shiftByType.end())
                shiftByType[type] = 0;
            auto shift = shiftByType[type];

            // Get the default position for the given type
            auto nodePosition = ImVec2(-1, -1);
            auto defaultPosIt = defaultPositionByType.find(type);
            if (defaultPosIt == defaultPositionByType.end())
            {
                type = "default";
                nodePosition = defaultPositionByType["default"];
            }
            else
            {
                nodePosition = defaultPosIt->second;
                nodePosition.x += shift;
            }
            
            ImGui::SetCursorPos(nodePosition);
            renderNode(name);

            shiftByType[type] += _nodeSize[0] + 8;
        }

        // Draw lines
        for (auto& object : objectLinks)
        {
            auto& name = object.first;
            auto& links = object.second;

            auto& currentNodePos = _nodePositions[name];
            auto firstPoint = ImVec2(currentNodePos[0] + canvasPos.x, currentNodePos[1] + canvasPos.y);

            for (auto& target : links)
            {
                auto targetIt = _nodePositions.find(target);
                if (targetIt == _nodePositions.end())
                    continue;

                auto& targetPos = targetIt->second;
                auto secondPoint = ImVec2(targetPos[0] + canvasPos.x, targetPos[1] + canvasPos.y);

                drawList->AddLine(firstPoint, secondPoint, 0xBB0088FF, 2.f);
            }
        }

        // end of the subwindow
        ImGui::EndChild();

        // Interactions
        if (ImGui::IsItemHovered())
        {
            _isHovered = true;

            ImGuiIO& io = ImGui::GetIO();
            if (io.MouseDownTime[0] > 0.0)
            {
                float dx = io.MouseDelta.x;
                float dy = io.MouseDelta.y;
                _viewShift[0] += dx;
                _viewShift[1] += dy;
            }
        }
        else
            _isHovered = false;
    }
}

/*************/
void GuiNodeView::renderNode(string name)
{
    auto pos = _nodePositions.find(name);
    if (pos == _nodePositions.end())
    {
        auto cursorPos = ImGui::GetCursorPos();
        _nodePositions[name] = vector<float>({cursorPos.x + _viewShift[0], cursorPos.y + _viewShift[1]});
    }
    else
    {
        auto& nodePos = (*pos).second;
        ImGui::SetCursorPos(ImVec2(nodePos[0] + _viewShift[0], nodePos[1] + _viewShift[1]));
    }

    // Beginning of node rendering
    ImGui::BeginChild(string("node_" + name).c_str(), ImVec2(_nodeSize[0], _nodeSize[1]), false);

    ImGui::SetCursorPos(ImVec2(0, 2));
    if (ImGui::CollapsingHeader(name.c_str()))
    {
    }

    if (ImGui::IsItemHovered())
    {
        ImGuiIO& io = ImGui::GetIO();
        if (io.MouseDownTime[0] > 0.0)
        {
            float dx = io.MouseDelta.x;
            float dy = io.MouseDelta.y;
            auto& nodePos = (*pos).second;
            nodePos[0] += dx;
            nodePos[1] += dy;
        }
    }
    
    // End of node rendering
    ImGui::EndChild();
}

/*************/
int GuiNodeView::updateWindowFlags()
{
    ImGuiWindowFlags flags = 0;
    if (_isHovered)
    {
        flags |= ImGuiWindowFlags_NoMove;
    }
    return flags;
}

} // end of namespace
