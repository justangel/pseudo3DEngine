//
// Created by ivan- on 18.01.2020.
//

#include "Camera.h"
#include "ClientUDP.h"
#include <cmath>
#include <algorithm>
#include <thread>
#include <mutex>

using namespace std;

Camera::Camera(World& world, Point2D position, double vPos, double height, double health, std::string texture, double fieldOfView, double eyesHeight, double depth, double walkSpeed, double jumpSpeed, double viewSpeed)
        : W_world(world), Player(position, vPos, height, health, texture), d_eyesHeight(eyesHeight), d_depth(depth), d_walkSpeed(walkSpeed), d_jumpSpeed(jumpSpeed), d_viewSpeed(viewSpeed)
{
    Weapon weapon1(300);
    weapon1.choiceWeapon("shotgun");
    v_weapons.push_back(weapon1);

    walkSound.setBuffer(*ResourceManager::loadSoundBuffer(WALK_SOUND));
    walkSound.setLoop(true);
    walkSound.setVolume(50.f);

    setFieldOfView(fieldOfView);

    curFrame.distances.resize(DISTANCES_SEGMENTS);
    oldFrame.distances.resize(DISTANCES_SEGMENTS);

    threadCount = thread::hardware_concurrency();
    threads.resize(threadCount);

    startM.lock();
    finished = 0;
    work = 1;
    for (int i = 0; i < threadCount; i++)
        threads[i] = make_shared<thread>(&Camera::updateThread, this, i, threadCount);
    startM.unlock();
    startCV.notify_all();
    d_aspectRatio = 1.0;
}

Camera::~Camera()
{
    startM.lock();
    work = -1;
    startM.unlock();

    startCV.notify_all();

    for (auto t : threads)
        if (t->joinable())
            t->join();
}

void Camera::setFieldOfView(double angle)
{
    if(angle < 0)
        d_fieldOfView = PI/4;
    else if(angle >= PI)
        d_fieldOfView = 4*PI/5;
    else
        d_fieldOfView = angle;

    for (int i = 0; i < DISTANCES_SEGMENTS; i++)
    {
        double halfWidth = tan(d_fieldOfView / 2) * ((double)SCREEN_WIDTH / SCREEN_HEIGHT);
        double offset = ((i * 2.0 / (DISTANCES_SEGMENTS - 1.0)) - 1.0) * halfWidth;
        Point2D dir = { 1, 1 * offset };
        dir = dir.normalize();
        double angle = atan2(dir.y, dir.x);
        horizontalCos[i] = cos(angle);
        horizontalSin[i] = sin(angle);
    }
    for (int i = 0; i < SCREEN_HEIGHT; i++)
    {
        double halfWidth = tan(d_fieldOfView / 2);
        double offset = ((i * 2.0 / SCREEN_HEIGHT) - 1.0) * halfWidth;
        Point2D dir = { 1, 1 * offset };
        dir = dir.normalize();
        verticalTan[i] = tan(atan2(dir.y, dir.x));
    }
}

//Camera::Camera(const Camera& camera) : Player(camera), W_world(camera.W_world)
//{
//    v_distances = camera.v_distances;
//    allCollisions = camera.allCollisions;
//    d_direction = camera.d_direction;
//    d_depth = camera.d_depth;
//    d_fieldOfView = camera.d_fieldOfView;
//    d_eyesHeight = camera.d_eyesHeight;
//    d_walkSpeed = camera.d_walkSpeed;
//    d_viewSpeed = camera.d_viewSpeed;
//    b_collision = camera.b_collision;
//    b_textures = camera.b_textures;
//    b_smooth = camera.b_smooth;
//    localMousePosition = camera.localMousePosition;
//    v_weapons = camera.v_weapons;
//    i_selectedWeapon = camera.i_selectedWeapon;
//    walkSound = camera.walkSound;
//}

bool Camera::isSmooth()
{
    return b_smooth;
}

void Camera::setSmooth(bool active)
{
    b_smooth = active;
}

bool Camera::isCollision()
{
    return b_collision;
}

void Camera::setCollision(bool active)
{
    b_collision = active;
}

bool Camera::isTextures()
{
    return b_textures;
}

void Camera::setTextures(bool active)
{
    b_textures = active;
}

// Just increase distance for all collisions in v_RayCastStructure
void Camera::recursiveIncreaseDistance(std::vector<RayCastStructure>& v_RayCastStructure, double distance)
{
    for (auto& rcs : v_RayCastStructure)
    {
        rcs.distance += distance;
        recursiveIncreaseDistance(rcs.v_mirrorRayCast, distance);
    }
}

// Add player to the list of existing players
void Camera::addPlayer(std::string name, std::shared_ptr<Player> camera)
{
    m_playersOnTheScreen.insert({ name, camera });
}

// Remove player from the list of existing players
void Camera::removePlayer(const std::string& name)
{
    m_playersOnTheScreen.erase(name);
}

// Find all objects hitted by ray, except object with given name.
// Repeats raycasting if mirror hitted and reflections < 40
void Camera::objectsRayCrossed(const pair<Point2D, Point2D>& ray, std::vector<RayCastStructure>& v_rayCastStruct, const Object2D* caster, int reflections)
{
    Object2D* obj = nullptr;
    pair<Point2D, Point2D> edge;
    Point2D nearCross;
    double len = 0;
    double closest = d_depth;
    for (auto& object : W_world.objects())
    {
        Object2D* pobject = object.second.get();
        if (pobject == caster || pobject == nullptr || pobject->nodes().size() < 2)
            continue;

        // Check crossing

        Point2D crossPoint = ray.second;
        std::vector<RayCastStructure> v_reflectedRayCastStructure;

        pair<Point2D, Point2D> wall;
        // If ray crossed with object
        if (pobject->cross(ray, wall, crossPoint, len))
        {
            // If it was mirror
            if (pobject->isMirror() && scalarWithNormal(wall.second - wall.first, ray.second - ray.first) < 0 && reflections < 40)
            {
                Point2D edgeVector = wall.second - wall.first;
                Point2D rayVector = ray.second - ray.first;
                double twistAngle = 2 * acos((edgeVector.x * rayVector.x + edgeVector.y * rayVector.y) / (edgeVector.abs() * rayVector.abs()));
                Point2D twistedRayVector = { rayVector.x * cos(twistAngle) + rayVector.y * sin(twistAngle), -rayVector.x * sin(twistAngle) + rayVector.y * cos(twistAngle) };
                pair<Point2D, Point2D> newRay = { crossPoint, crossPoint + twistedRayVector };

                objectsRayCrossed(newRay, v_reflectedRayCastStructure, pobject, reflections + 1);
                recursiveIncreaseDistance(v_reflectedRayCastStructure, (ray.first - crossPoint).abs());
            }
            v_rayCastStruct.push_back({ (ray.first - crossPoint).abs(), len, pobject, pobject->height(), v_reflectedRayCastStructure });
            // for collision
            double dist = (crossPoint - position()).abs();
            // Processing bonuses, because they are walkable
            if (pobject->type() == ObjectType::Bonus)
            {
                if (caster == this && dist < COLLISION_AREA)
                {
                    bonusM.lock();
                    if (find(curFrame.bonuses.begin(), curFrame.bonuses.end(), reinterpret_cast<Bonus*>(pobject)) == curFrame.bonuses.end())
                        curFrame.bonuses.push_back(reinterpret_cast<Bonus*>(pobject));
                    bonusM.unlock();
                }
            }
                // Remembering all other objects
            else if (dist < closest)
            {
                edge = std::move(wall);
                closest = dist;
                nearCross = crossPoint;
                obj = pobject;
            }
        }
    }
    // Sort hits by descending of distance
    std::sort(v_rayCastStruct.begin(), v_rayCastStruct.end(), [](const RayCastStructure& lh, const RayCastStructure& rh) { return lh.distance > rh.distance; });

    // collision
    if (b_collision && obj != nullptr && caster == this && closest <= COLLISION_AREA)
    {
        //if (obj->type() == ObjectType::Bonus)
        //    throw std::exception("WTF");
        CollisionInformation newCollision;
        newCollision.distance = (nearCross - position()).abs();
        newCollision.edge = std::move(edge);
        newCollision.collisionPoint = nearCross;
        newCollision.height = obj->height();
        collisionM.lock();
        curFrame.collisions.push_back(newCollision);
        collisionM.unlock();
    }
}

// Find objects crossed by ray near enough for collisions.
// Must be use only on invisible segments.
void Camera::hiddenObjectsRayCrossed(const pair<Point2D, Point2D>& ray, const Object2D* caster)
{
    Object2D* obj = nullptr;
    std::pair<Point2D, Point2D> edge;
    Point2D nearCross = ray.second;
    double len = 0;
    double closest = d_depth;
    for (auto& object : W_world.objects())
    {
        Object2D* pobject = object.second.get();
        if (pobject == caster || pobject->nodes().size() < 2)
            continue;

        // Check collision

        Point2D crossPoint = ray.second;

        // If object hitted and near closer than already finded - rember it
        pair<Point2D, Point2D> wall;
        // Remembering all other objects
        if (pobject->cross(ray, wall, crossPoint, len))
        {
            double dist = (crossPoint - ray.first).abs();
            if (pobject->type() == ObjectType::Bonus)
            {
                bonusM.lock();
                if (find(curFrame.bonuses.begin(), curFrame.bonuses.end(), reinterpret_cast<Bonus*>(pobject)) == curFrame.bonuses.end())
                    curFrame.bonuses.push_back(reinterpret_cast<Bonus*>(pobject));
                bonusM.unlock();
            }
            else if (closest > dist)
            {
                nearCross = std::move(crossPoint);
                obj = pobject;
                edge = std::move(wall);
                closest = dist;
            }
        }
    }
    // If something was hitted close enough - write it
    if (obj)
    {
        //if (obj->type() == ObjectType::Bonus)
        //    throw std::exception("WTF");
        CollisionInformation newCollision;
        newCollision.distance = (nearCross - position()).abs();
        newCollision.edge = std::move(edge);
        newCollision.collisionPoint = nearCross;
        newCollision.height = obj->height();
        collisionM.lock();
        curFrame.collisions.push_back(newCollision);
        collisionM.unlock();
    }
}

void Camera::updateDistances_from_to(int from, int to)
{
    Point2D forward = { cos(d_direction), sin(d_direction) };
    Point2D left = { -forward.y, forward.x };
    double halfWidth = tan(d_fieldOfView / 2) * ((double)SCREEN_WIDTH / SCREEN_HEIGHT);

    // Visible for player segments
    for (int i = from; i < to; i++) // Need top-down map? Set limit to segs. No need? DISTANCE_SEGMENTS.
    {
        pair<Point2D, Point2D> segment1;

        double offset = ((i * 2.0 / (DISTANCES_SEGMENTS - 1.0)) - 1.0) * halfWidth;
        segment1 = { {x(), y()}, {x() + d_depth * (forward.x + offset * left.x), y() + d_depth * (forward.y + offset * left.y)} };

        std::vector<RayCastStructure> v_rayCastStructure;

        objectsRayCrossed(segment1, v_rayCastStructure, this);

        if (v_rayCastStructure.empty())
            v_rayCastStructure.emplace_back(RayCastStructure{ d_depth, 0, nullptr, 0 });
        curFrame.distances[i] = v_rayCastStructure;
    }
}

void Camera::updateHiddenDistances_from_to(int from, int to)
{
    for (int i = from; i < to; i += 10)
    {
        pair<Point2D, Point2D> segment1;

        double direction = d_direction + ((double)i / DISTANCES_SEGMENTS - 0.5) * d_fieldOfView;
        segment1 = { {x(), y()}, {x() + COLLISION_AREA * cos(direction), y() + COLLISION_AREA * sin(direction)} };

        hiddenObjectsRayCrossed(segment1, this);
    }
}

void Camera::updateThread(int i, int n)
{
    int segs = static_cast<int>(2 * PI / d_fieldOfView * DISTANCES_SEGMENTS);
    int step1 = static_cast<int>(curFrame.distances.size() / n);
    int step2 = static_cast<int>((segs - DISTANCES_SEGMENTS) / n);

    int from1 = i * step1;
    int from2 = DISTANCES_SEGMENTS + i * step2;
    int to1 = from1 + step1;
    int to2 = from2 + step2;
    if (i == n - 1)
    {
        int to1 = curFrame.distances.size();
        int to2 = segs;
    }

    signed char lastWork = 1;
    unique_lock<mutex> lk(startM);
    while (true)
    {
        startCV.wait(lk, [this, lastWork] {return work != lastWork; });
        lastWork = work;
        lk.unlock();
        // Camera destructor
        if (work == -1)
            return;
        // Work
        updateDistances_from_to(from1, to1);
        updateHiddenDistances_from_to(from2, to2);

        // Notify main thread about finished work
        ++finished;
        endCV.notify_one();
        lk.lock();
    }
}

// Check all directions for collisions and walls
void Camera::updateDistances()
{
    throw std::logic_error("updateDistances(const World&) method currenly replaced by synchronized multithreading.");
}

void Camera::startFrameProcessing()
{
    signed char newWork = 1 - work;
    finished = 0;
    work = newWork;
    startCV.notify_all();
}

void Camera::endFrameProcessing()
{
    unique_lock<mutex> lk(endM);
    while (finished != threadCount)
        endCV.wait(lk, [this] {return finished == threadCount; });
    std::swap(oldFrame, curFrame);
    oldFrame.direction = d_direction;
    oldFrame.position = p_position;
    oldFrame.height = d_vPos;
    for (auto b : curFrame.bonuses)
    {
        if (b->bonusType() == BonusType::TreatmentBonus)
            client->shoot(getName(), -100, 1);
        if (b->bonusType() == BonusType::AmmunitionBonus)
            v_weapons[i_selectedWeapon].add(15);
        if (b->bonusType() == BonusType::ViewBonus)
            if (d_fieldOfView < PI / 2)
                setFieldOfView(d_fieldOfView + (double)PI / 20);
        if (b->bonusType() == BonusType::SpeedBonus) {
            if (d_walkSpeed < 7)
                d_walkSpeed += 0.5;
            if (d_jumpSpeed < 10)
                d_jumpSpeed++;
        }
    }
    for (auto b : oldFrame.bonuses)
    {
        W_world.freeBonusPoint(b->position());                 // free this place for another bonus
        b->setPosition(W_world.getBonusPoint());  // change the position of this bonus and mark this
        // position as busy.
    }
    oldFrame.enemies.resize(m_playersOnTheScreen.size());
    auto it1 = m_playersOnTheScreen.begin();
    for (int i = oldFrame.enemies.size(); i-- > 0; ++it1)
    {
        oldFrame.enemies[i].position = it1->second->position();
        oldFrame.enemies[i].vPos = it1->second->vPos();
        oldFrame.enemies[i].height = it1->second->height();
        oldFrame.enemies[i].health = it1->second->health();
    }
    curFrame.collisions.clear();
    curFrame.bonuses.clear();
}

// Draw camera on 2d map
void Camera::draw(sf::RenderTarget& window)
{
    if (oldFrame.distances.empty() || health() <= 0)
        return;

    sf::CircleShape circle;
    circle.setRadius(COLLISION_DISTANCE * SCALE);
    circle.setOutlineColor(OUTLINE_CAMERA_COLOR);
    circle.setFillColor(FILL_CAMERA_COLOR);
    circle.setOutlineThickness(OUTLINE_CAMERA_THICKNESS);
    circle.setPosition((float)x() * SCALE - COLLISION_DISTANCE * SCALE, (float)y() * SCALE - COLLISION_DISTANCE * SCALE);

    double left = oldFrame.direction - d_fieldOfView / 2;
    double right = oldFrame.direction + d_fieldOfView / 2;

    sf::ConvexShape triangle;
    triangle.setPointCount(CONVEX_NUMBER + 2);
    triangle.setPoint(0, sf::Vector2f(0, 0));
    for (int i = 0; i <= CONVEX_NUMBER; i++)
    {
        int index = oldFrame.distances[(int)i * DISTANCES_SEGMENTS / CONVEX_NUMBER].size() - 1;
        triangle.setPoint(i + 1, sf::Vector2f(static_cast<float>(oldFrame.distances[i * DISTANCES_SEGMENTS / CONVEX_NUMBER][index].distance * cos(left + (right - left) * i / CONVEX_NUMBER) * SCALE), static_cast<float>(oldFrame.distances[i * DISTANCES_SEGMENTS / CONVEX_NUMBER][index].distance * sin(left + (right - left) * i / CONVEX_NUMBER) * SCALE)));
    }
    triangle.setOutlineColor(OUTLINE_CAMERA_COLOR);
    triangle.setFillColor(FILED_OF_VEW_COLOR);
    triangle.setOutlineThickness(OUTLINE_CAMERA_THICKNESS);
    triangle.setPosition((float)x() * SCALE, (float)y() * SCALE);

    window.draw(triangle);
    window.draw(circle);
}

// Returns player if ray hits him
std::pair<Object2D*, double> Camera::cameraRayCheck(RayCastStructure& structure)
{
    std::pair<Object2D*, double> result = { nullptr, 1 };

    // If hit player
    if (structure.object->type() == ObjectType::Player)
    {
        auto heights = heightInPixels(structure.distance, structure.object->height(), dynamic_cast<Player*>(structure.object)->vPos());
        heights.first = std::min(mirrorBot, std::max(mirrorTop, heights.first));
        heights.second = std::min(mirrorBot, std::max(mirrorTop, heights.second));
        if (heights.first < SCREEN_HEIGHT / 2 && heights.second > SCREEN_HEIGHT / 2)
        {
            result.first = structure.object;
            result.second = structure.distance;
        }
    }
        // If hit mirror
    else if (!structure.v_mirrorRayCast.empty())
    {
        auto heights = heightInPixels(structure.distance, structure.object->height(), 0);
        double mirrorTopSave = mirrorTop;
        double mirrorBotSave = mirrorBot;
        mirrorTop = std::min(mirrorBotSave, std::max(mirrorTopSave, heights.first));
        mirrorBot = std::min(mirrorBotSave, std::max(mirrorTopSave, heights.second));
        result = cameraRayCheck(structure.v_mirrorRayCast[structure.v_mirrorRayCast.size() - 1]);
        mirrorTop = mirrorTopSave;
        mirrorBot = mirrorBotSave;
    }
    return result;
}

// Fire at the player
void Camera::fire()
{
    pair<Point2D, Point2D> segment1 = { {x(), y()}, {x() + d_depth * cos(oldFrame.direction), y() + d_depth * sin(oldFrame.direction)} };
    std::vector<RayCastStructure> v_rayCastStructure;
    objectsRayCrossed(segment1, v_rayCastStructure, this);

    // if something hitted
    if (!v_rayCastStructure.empty())
    {
        std::pair<Object2D*, double> hitted = cameraRayCheck(v_rayCastStructure[v_rayCastStructure.size() - 1]);
        if (hitted.first && abs(reinterpret_cast<Player*>(hitted.first)->vPos() - oldFrame.height) < d_eyesHeight)
            client->shoot(hitted.first->getName(), v_weapons[i_selectedWeapon].damage(), hitted.second);
    }
}

// Read keyboard and mouse input
bool Camera::keyboardControl(double elapsedTime, sf::RenderWindow& window)
{
    // Prevents control from another window
    if (!window.hasFocus())
    {
        b_hadFocus = false;
        walkSound.pause();
        double newHeight = d_vPos + d_vSpeed * elapsedTime;
        if (newHeight > 0)
            d_vSpeed -= 9.8 * elapsedTime;
        else
        {
            newHeight = 0;
            d_vSpeed = 0;
        }
        setVPos(newHeight);
        return true;
    }

    // Exit to menu
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Escape))
    {
        b_hadFocus = false;
        return false;
    }

    double dx = 0;
    double dy = 0;
    double d_sin = sin(oldFrame.direction);
    double d_cos = cos(oldFrame.direction);
    // Left and right
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::A))
    {
        dx += d_sin;
        dy -= d_cos;
    }
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::D))
    {
        dx -= d_sin;
        dy += d_cos;
    }
    // Forward and backward
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::W))
    {
        dx += d_cos;
        dy += d_sin;
    }
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::S))
    {
        dx -= d_cos;
        dy -= d_sin;
    }

    if(sf::Keyboard::isKeyPressed(sf::Keyboard::G)) {
        b_godMode = !b_godMode;
        if(b_godMode) {
            d_walkSpeed = 10;
            v_weapons[i_selectedWeapon].set(30000);
        } else {
            d_walkSpeed = 3;
        }
    }

    if(sf::Keyboard::isKeyPressed(sf::Keyboard::LShift) && b_godMode) {
        setVPos(vPos() - 0.1);
    }

    if (vPos() == 0 && sf::Keyboard::isKeyPressed(sf::Keyboard::Space) && !b_godMode)
    {
        d_vSpeed = d_jumpSpeed * (health() / 1000 * 7 + 0.3); // health modificator from 0.3 to 1 => 0.3 + (health / 100) * 0.7
    }

    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Space) && b_godMode)
    {
        setVPos(vPos() + 0.1);
    }

    // Fire
    if (sf::Mouse::isButtonPressed(sf::Mouse::Left))
    {
        if (v_weapons[i_selectedWeapon].fire())
            fire();
    }

    // Mouse movement
    int differenceX = sf::Mouse::getPosition(window).x - (int)window.getSize().x / 2;
    int differenceY = sf::Mouse::getPosition(window).y - (int)window.getSize().y / 2;
    sf::Mouse::setPosition({ (int)window.getSize().x / 2, (int)window.getSize().y / 2 }, window);
    // Ignoring first frame after window focus or game start
    if (b_hadFocus)
    {
        d_direction += d_viewSpeed * differenceX;
        d_verticalShift += 0*d_viewSpeed * differenceY;
    }

    // Start/stop walk sound
    if ((dx * dx + dy * dy) > d_walkSpeed * elapsedTime * d_walkSpeed * elapsedTime / 10 && vPos() <= 0)
    {
        if (walkSound.getStatus() != sf::Sound::Status::Playing)
            walkSound.play();
    }
    else {
        walkSound.pause();
    }

    // Move player
    shiftPrecise({ dx * d_walkSpeed * elapsedTime * (health() / 100), dy * d_walkSpeed * elapsedTime * (health() / 100) }, d_vSpeed * elapsedTime);

    if(!b_godMode)
        d_vSpeed -= 9.8 * elapsedTime;
    else
        setHealth(100);

    // Remember that we had focus and weren't in menu
    b_hadFocus = true;
    return true;
}

// Returns position of wall on screen
pair<double, double> Camera::heightInPixels(double distance, double height, double vertical)
{
    pair<double, double> h = { 0, 0 };
    double mod = tan(d_fieldOfView / 2) * distance;
    h.first = (1 - (height - d_eyesHeight - oldFrame.height + vertical) / mod) * SCREEN_HEIGHT / 2;
    h.second = (1 + (d_eyesHeight + oldFrame.height - vertical) / mod) * SCREEN_HEIGHT / 2;
    return h;
}

// Draw 1 pixel column of screen
void Camera::drawVerticalStrip(sf::RenderTarget& window, const RayCastStructure& obj, int shift, int f)
{
    int horIndex = shift / (SCREEN_WIDTH / DISTANCES_SEGMENTS);
    int h2;
    if (obj.object == nullptr)
        h2 = SCREEN_HEIGHT / 2;
    else
    {
        sf::ConvexShape polygon;
        polygon.setPointCount(4);

        double vertical = 0;
        if (obj.object->type() == ObjectType::Player)
            vertical = dynamic_cast<Player*>(obj.object)->vPos();
        pair<double, double> height_now = heightInPixels(obj.distance * horizontalCos[horIndex], obj.height, vertical);

        int h1  = (int)std::max(mirrorTop, std::min(mirrorBot, height_now.first));
        h2      = (int)std::min(mirrorBot, std::max(mirrorTop, height_now.second));

        polygon.setPoint(0, sf::Vector2f(0, (float)h1));
        polygon.setPoint(1, sf::Vector2f(0, (float)h2));
        polygon.setPoint(2, sf::Vector2f((float)SCREEN_WIDTH / DISTANCES_SEGMENTS, (float)h2));
        polygon.setPoint(3, sf::Vector2f((float)SCREEN_WIDTH / DISTANCES_SEGMENTS, (float)h1));

        int alpha = (int)(255 * (1 - obj.distance / d_depth / 2));
        if (alpha > 255)
            alpha = 255;
        if (alpha < 0)
            alpha = 0;

        //alpha = 255 - alpha;

        if (!b_textures)
            polygon.setFillColor({ 255, 174, 174, static_cast<sf::Uint8>(255 - alpha) });
        else
            polygon.setFillColor({ 255, 255, 255, static_cast<sf::Uint8>(255 - alpha) });

        //polygon.setOutlineThickness(0); // we can make non zero thickness for debug
        polygon.setPosition((float)(shift * SCREEN_WIDTH / DISTANCES_SEGMENTS), d_verticalShift);

        sf::Sprite sprite;
        if (obj.object && b_textures)
        {
            sprite.setPosition(sf::Vector2f((float)shift * SCREEN_WIDTH / DISTANCES_SEGMENTS, (float)h1 + d_verticalShift)); // absolute position
            int left;
            int top, finalTop;
            int bot, finalBot;
            if (obj.object->isMirror()) // In case of mirror
            {
                sprite.setTexture(W_world.skyTexture());
                left = (int)(oldFrame.direction / 10 * SCREEN_WIDTH);
                top = sprite.getTextureRect().height/2 - SCREEN_HEIGHT/2 + 250;
                bot = sprite.getTextureRect().height/2 + SCREEN_HEIGHT/2 + 250;
                double scaleFactor = (double)SCREEN_HEIGHT / (sprite.getTextureRect().height + 540);
                sprite.scale(1, (float)scaleFactor);
            }
            else
            {
                sprite.setTexture(obj.object->loadTexture());
                left = obj.object->aspect() * obj.progress * SCREEN_WIDTH;
                top = 0;
                bot = obj.object->loadTexture().getSize().y;
                sprite.scale(1, (float)(height_now.second - height_now.first) / obj.object->loadTexture().getSize().y);
            }

            if ((int)height_now.first != h1)
                finalTop = top + (bot - top) * (h1 - height_now.first) / (height_now.second - height_now.first);
            else
                finalTop = top;

            if ((int)height_now.second != h2)
                finalBot = top + (bot - top) * (h2 - height_now.first) / (height_now.second - height_now.first);
            else
                finalBot = bot;

            sprite.setColor({ static_cast<sf::Uint8>(alpha), static_cast<sf::Uint8>(alpha), static_cast<sf::Uint8>(alpha), 255 });
            sprite.setTextureRect(sf::IntRect(left, finalTop, SCREEN_WIDTH / DISTANCES_SEGMENTS, finalBot - finalTop));
            renderM.lock();
            window.draw(sprite);
            renderM.unlock();
        }

        if(!b_textures)
            if (abs(obj.distance - d_depth) > 0.001) {
                renderM.lock();
                window.draw(polygon);
                renderM.unlock();
            }

        mirrorTop = h1;
        mirrorBot = h2;
    }

    // Floor drawing

    if(b_godMode)
        return;

    int x = shift * SCREEN_WIDTH / DISTANCES_SEGMENTS;

    if (b_smooth || (f == 0) || (x % FLOOR_SEGMENT_SIZE != 0) || !b_textures) return;

    const int scale = 400;
    double baseOffset = (d_eyesHeight + oldFrame.height) / horizontalCos[horIndex];
    double horMod = horizontalCos[horIndex] * directionCos - horizontalSin[horIndex] * directionSin;// cos (horizontalAngles[shift] + d_direction) = cos a * cos b - sin a * sin b
    double verMod = horizontalSin[horIndex] * directionCos + horizontalCos[horIndex] * directionSin;// sin (horizontalAngles[shift] + d_direction) = sin a * cos b + cos a * sin b

    for (int z = h2 + d_verticalShift; z < SCREEN_HEIGHT; z += FLOOR_SEGMENT_SIZE)
    {
        double offset = baseOffset / verticalTan[z];
        int left = (int)(scale * (oldFrame.position.x + offset * horMod));
        int top = (int)(scale * (oldFrame.position.y + offset * verMod));

        int alpha2 = 255 * 5 * (z - SCREEN_HEIGHT / 5) / SCREEN_HEIGHT / 4;

        //if(alpha2 > 255)
        //    alpha2 = 255;


        sf::Sprite& floor = W_world.floor();

        floor.setTextureRect(sf::IntRect(left, top, FLOOR_SEGMENT_SIZE, FLOOR_SEGMENT_SIZE));
        floor.setPosition(sf::Vector2f((float)x, (float)z)); // absolute position
        floor.setColor({ 255, 255, 255, static_cast<sf::Uint8>(alpha2) });
        renderM.lock();
        window.draw(floor);
        renderM.unlock();
    }
}

void Camera::recursiveDrawing(sf::RenderTarget& window, const std::vector<RayCastStructure>& v_RayCastStructure, int shift, int rec)
{
    int i = 0;
    double mirrorTopSave = mirrorTop;
    double mirrorBotSave = mirrorBot;
    for (const auto& k : v_RayCastStructure)
    {
        if (i + 1 != v_RayCastStructure.size() || (rec != 1))
            drawVerticalStrip(window, k, shift, 0);
        else
            drawVerticalStrip(window, k, shift, 1);
        i++;
        if (!k.v_mirrorRayCast.empty())
            recursiveDrawing(window, k.v_mirrorRayCast, shift, 2);
        mirrorTop = mirrorTopSave;
        mirrorBot = mirrorBotSave;
    }
}

void Camera::recursiveDrawing_from_to(sf::RenderTarget* window, int from, int to) {
    for (int i = from; i < to; i++)
        recursiveDrawing(*window, oldFrame.distances[i], i);
}

void Camera::drawHealth(sf::RenderTarget& window, int xPos, int yPos, int width, int healthProgress)
{
    sf::ConvexShape polygon1;
    polygon1.setPointCount(4);
    sf::ConvexShape polygon2;
    polygon2.setPointCount(4);

    polygon1.setPoint(0, sf::Vector2f((float)xPos, (float)yPos));
    polygon1.setPoint(1, sf::Vector2f((float)xPos + width, (float)yPos));
    polygon1.setPoint(2, sf::Vector2f((float)xPos + width, (float)yPos + 20));
    polygon1.setPoint(3, sf::Vector2f((float)xPos, (float)yPos + 20));

    polygon2.setPoint(0, sf::Vector2f((float)xPos, (float)yPos));
    polygon2.setPoint(1, sf::Vector2f((float)xPos + width * healthProgress / 100, (float)yPos));
    polygon2.setPoint(2, sf::Vector2f((float)xPos + width * healthProgress / 100, (float)yPos + 20));
    polygon2.setPoint(3, sf::Vector2f((float)xPos, (float)yPos + 20));

    polygon1.setFillColor({ 255, 174, 174, 100 });
    polygon2.setFillColor({ static_cast<sf::Uint8>((100 - healthProgress) * 255), static_cast<sf::Uint8>(healthProgress * 255 / 100), 0, 100 });

    polygon1.setOutlineThickness(3); // we can make non zero thickness for debug
    window.draw(polygon1);
    window.draw(polygon2);
}

void Camera::drawCameraView(sf::RenderTarget& window)
{
    if (oldFrame.distances.empty())
        return;

    directionSin = sin(oldFrame.direction);
    directionCos = cos(oldFrame.direction);

    // Draw sky and rotate floor
    if (b_textures)
    {
        sf::Sprite sprite_sky;
        sprite_sky.setTexture(W_world.skyTexture());
        sprite_sky.setTextureRect(sf::IntRect((int)(oldFrame.direction * SCREEN_WIDTH / 2), sprite_sky.getTextureRect().height - SCREEN_HEIGHT/2 - 540, SCREEN_WIDTH, SCREEN_HEIGHT));
        sprite_sky.setPosition(sf::Vector2f(0, d_verticalShift)); // absolute position
        window.draw(sprite_sky);
        W_world.floor().setRotation(static_cast<float>(-oldFrame.direction / PI * 180 - 90));

        if (b_smooth)
        {
            sf::Image floorPrerendered;
            floorPrerendered.create(SCREEN_WIDTH, SCREEN_HEIGHT, sf::Color(0, 0, 0, 0));
            sf::Image floorTexture = W_world.floorTexture().copyToImage();
            const int scale = 400;
            for (int i = 0; i < SCREEN_WIDTH; i++)
            {
                int horIndex = i / (SCREEN_WIDTH / DISTANCES_SEGMENTS);
                double baseOffset = (d_eyesHeight + oldFrame.height) / horizontalCos[horIndex];
                double horMod = horizontalCos[horIndex] * directionCos - horizontalSin[horIndex] * directionSin;
                double verMod = horizontalSin[horIndex] * directionCos + horizontalCos[horIndex] * directionSin;
                for (int j = SCREEN_HEIGHT / 2; j < SCREEN_HEIGHT; j++)
                {
                    double offset = baseOffset / verticalTan[j];
                    int left = (int)(scale * (oldFrame.position.x + offset * horMod)) % 960;
                    int top = (int)(scale * (oldFrame.position.y + offset * verMod)) % 960;
                    left += (left < 0) * 960;
                    top += (top < 0) * 960;
                    int alpha2 = 255 * (j * 2 - SCREEN_HEIGHT) / SCREEN_HEIGHT;
                    sf::Color col = floorTexture.getPixel(left, top);
                    col.a = alpha2;
                    floorPrerendered.setPixel(i, j, col);
                }
            }
            sf::Texture floorText;
            floorText.loadFromImage(floorPrerendered);
            sf::Sprite floorSprite(floorText);
            window.draw(floorSprite);
        }
    }

    // ------- SEVERAL THREADS ------
    /*
    window.setActive(false);
    vector<shared_ptr<thread>> threads;
    for(int i = 0; i < thread::hardware_concurrency(); i++) {
        int step = static_cast<int>(DISTANCES_SEGMENTS/thread::hardware_concurrency());
        int from    = i * step;
        int to      = from + step;
        if(i == thread::hardware_concurrency() - 1)
            to      = DISTANCES_SEGMENTS;
        threads.emplace_back(shared_ptr<thread>(new thread(&Camera::recursiveDrawing_from_to, this, &window, from, to)));
    }
    for(auto& t : threads)
        if(t->joinable())
            t->join();
    threads.clear();
    */
    // ------------ END -------------

    // One thread style:

    for (int i = 0; i < DISTANCES_SEGMENTS; i++)
    {
        recursiveDrawing(window, oldFrame.distances[i], i);
    }


    double halfWidth = tan(d_fieldOfView / 2) * ((double)SCREEN_WIDTH / SCREEN_HEIGHT);
    //m_playersOnTheScreen
    for (const auto& player : oldFrame.enemies)
    {
        bool canSee = true;
        Point2D enemyDirection = (player.position - oldFrame.position).normalize();

        pair<Point2D, Point2D> segment1 = { {x(), y()}, {x() + d_depth*enemyDirection.x, y() + d_depth*enemyDirection.y} };
        std::vector<RayCastStructure> v_rayCastStructure;
        objectsRayCrossed(segment1, v_rayCastStructure, this);
        //if(!v_rayCastStructure.empty() && v_rayCastStructure.back().object->getName() == player.first)
        //    canSee = true;


        enemyDirection = { enemyDirection.x * cos(-oldFrame.direction) - enemyDirection.y * sin(-oldFrame.direction), enemyDirection.x * sin(-oldFrame.direction) + enemyDirection.y * cos(-oldFrame.direction) };
        double offset = enemyDirection.y / enemyDirection.x;
        if (abs(offset) < halfWidth && enemyDirection.x > 0 && canSee)
        {
            int xPos = (int)((enemyDirection.y / enemyDirection.x / halfWidth + 1.0) / 2.0 * (DISTANCES_SEGMENTS - 1.0));
            int yPos = -30 + (int)(heightInPixels((player.position - oldFrame.position).abs(), player.height + 0.2, player.vPos).first);
            auto healthProgress = player.health;
            drawHealth(window, xPos - 50, yPos, 100, (int)healthProgress);
        }
    }
    if(b_godMode)
        return;

    drawHealth(window, 80, SCREEN_HEIGHT - 50, 400, (int)health());
    v_weapons[i_selectedWeapon].draw(window);

    W_world.health().setPosition(sf::Vector2f(-20, SCREEN_HEIGHT - 102)); // absolute position
    window.draw(W_world.health());

    W_world.shoot().setPosition(sf::Vector2f(SCREEN_WIDTH - 260, SCREEN_HEIGHT - 110)); // absolute position
    window.draw(W_world.shoot());

    sf::Text Text_health;
    sf::Text Text_shoots;
    sf::Text Text_kills_deaths;

    Text_health.setFont(W_world.font());
    Text_shoots.setFont(W_world.font());
    Text_kills_deaths.setFont(W_world.font());

    Text_health.setCharacterSize(20);
    Text_health.setFillColor(sf::Color::White);
    Text_health.setStyle(sf::Text::Bold);
    Text_health.setPosition(270, SCREEN_HEIGHT - 53);

    Text_shoots.setCharacterSize(80);
    Text_shoots.setFillColor(sf::Color::White);
    Text_shoots.setStyle(sf::Text::Bold);
    Text_shoots.setPosition(SCREEN_WIDTH - 130, SCREEN_HEIGHT - 102);

    Text_kills_deaths.setCharacterSize(40);
    Text_kills_deaths.setFillColor(sf::Color::White);
    Text_kills_deaths.setStyle(sf::Text::Bold);
    Text_kills_deaths.setPosition(20, 0);

    Text_health.setString(to_string((int)health()));
    Text_shoots.setString(to_string((int)v_weapons[i_selectedWeapon].balance()));

    Text_kills_deaths.setString("kills: " + to_string(kills()) + " | deaths: " + to_string(deaths()));

    window.draw(Text_health);
    window.draw(Text_shoots);
    window.draw(Text_kills_deaths);
}

double Camera::scalarWithNormal(Point2D edge, Point2D vector)
{
    Point2D normal = { edge.y, -edge.x };
    normal = normal.normalize();
    double scalar = vector.x * normal.x + vector.y * normal.y;
    return scalar;
}

void Camera::shiftPrecise(Point2D vector, double vertical)
{
    setVPos(vPos() + vertical);
    if (vPos() < 0)
    {
        d_vSpeed = 0;
        if(!b_godMode)
            setVPos(0);
    }
    if (!b_collision)
    {
        shift(vector);
        return;
    }

    //setVPos(d_eyesHeight - 0.1);

    for (auto& c : oldFrame.collisions)
    {
        Point2D edgeVector = c.edge.second - c.edge.first;
        Point2D normal = { edgeVector.y, -edgeVector.x };
        normal = normal.normalize();

        Point2D toWallVector = c.edge.first + c.edge.second - p_position * 2;
        if(normal * toWallVector > 0)
            normal = normal * (-1);

        double scalar = vector * normal;

        if (scalar < 0 && abs(c.distance - abs(scalar)) < COLLISION_DISTANCE && vPos()/* + d_eyesHeight*/ < c.height)
        {
            vector.x -= normal.x * scalar;
            vector.y -= normal.y * scalar;
        }

    }

    shift(vector);
}

void Camera::previousWeapon()
{
    if (i_selectedWeapon > 0)
        i_selectedWeapon--;
    else
        i_selectedWeapon = v_weapons.size() - 1;
}

void Camera::nextWeapon()
{
    if (i_selectedWeapon < (int)v_weapons.size() - 1)
        i_selectedWeapon++;
    else
        i_selectedWeapon = 0;
}