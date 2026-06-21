#pragma once
#include <string>
#include <cmath>
#include "scene/LightSource.h"

class Sun : public LightSource
{
public:
    float TimeOfDay = 12.0f;       // 0-24 hours
    float SunSensitivity = 1.0f;   // intensity multiplier

    Sun(const std::string& name)
        : LightSource(name)
    {
        ClassName = "Sun";
        Type = LightType::Directional;
        CastShadow = true;
        Color = Vector3D(1.0f, 0.97f, 0.88f);  // warm sunlight
        Range = 100.0f;
        UpdateFromTime();
    }

    void UpdateFromTime()
    {
        float phase = (TimeOfDay - 6.0f) / 12.0f * 3.14159265f;
        float elevation = sinf(phase);

        // Sun direction (from sun toward scene)
        Direction.X = -cosf(phase);
        Direction.Y = -elevation;
        Direction.Z = 0.0f;

        // Normalize
        float dlen = sqrtf(Direction.X * Direction.X + Direction.Y * Direction.Y + Direction.Z * Direction.Z);
        if (dlen > 0.001f) { Direction.X /= dlen; Direction.Y /= dlen; Direction.Z /= dlen; }

        // Intensity: 0 at night, full at noon, scaled by sensitivity
        float baseIntensity = std::max(0.0f, elevation);
        Intensity = baseIntensity * SunSensitivity;
    }

    Vector3D GetSunPosition(float distance = 300.0f) const
    {
        float phase = (TimeOfDay - 6.0f) / 12.0f * 3.14159265f;
        return Vector3D(cosf(phase) * distance, sinf(phase) * distance, 0.0f);
    }

    bool IsAboveHorizon() const
    {
        float phase = (TimeOfDay - 6.0f) / 12.0f * 3.14159265f;
        return sinf(phase) > 0.0f;
    }

    std::shared_ptr<GameObject> Clone() const override
    {
        auto clone = std::make_shared<Sun>(Name);
        CopyTo(clone.get());
        return clone;
    }

    void CopyTo(GameObject* target) const override
    {
        LightSource::CopyTo(target);
        if (target->ClassName == "Sun") {
            auto* sun = static_cast<Sun*>(target);
            sun->TimeOfDay = TimeOfDay;
            sun->SunSensitivity = SunSensitivity;
        }
    }
};
