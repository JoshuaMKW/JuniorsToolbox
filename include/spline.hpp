#pragma once

#include <cmath>
#include <vector>

namespace Toolbox {

    class ISpline2D {
    public:
        using ControlPoint = std::pair<float, float>;

        struct PathPoint {
            float m_x;
            float m_y;
            float m_tan_rotation;
        };

    public:
        virtual ~ISpline2D() = default;

        virtual bool isEmpty() const                                          = 0;
        virtual float getLength() const                                       = 0;
        virtual PathPoint getInterpolatedPoint(float t) const                 = 0;
        virtual PathPoint getDiscretePoint(size_t index) const                = 0;
        virtual float getInterpolationFromPoint(const PathPoint &point) const = 0;
        virtual float getInterpolationFromLength(float length) const          = 0;
        virtual float getLengthFromInterpolation(float t) const               = 0;

        virtual void setTranslate(float x, float y) = 0;
        virtual void setRotate(float radians)       = 0;
        virtual void setScale(float scale)          = 0;
    };

    class CatmullSpline2D : public ISpline2D {
    public:
        CatmullSpline2D() = delete;
        CatmullSpline2D(const std::vector<ControlPoint> &control_points, size_t resolution = 20);

    public:
        CatmullSpline2D(const CatmullSpline2D &) = default;
        CatmullSpline2D(CatmullSpline2D &&)      = default;
        ~CatmullSpline2D()                       = default;

        CatmullSpline2D &operator=(const CatmullSpline2D &) = default;
        CatmullSpline2D &operator=(CatmullSpline2D &&)      = default;

        bool isEmpty() const override { return m_path_points.empty(); }
        float getLength() const override;
        PathPoint getInterpolatedPoint(float t) const override;
        PathPoint getDiscretePoint(size_t index) const override;
        float getInterpolationFromPoint(const PathPoint &point) const override;
        float getInterpolationFromLength(float length) const override;
        float getLengthFromInterpolation(float t) const override;

        void setTranslate(float x, float y) override {
            m_translate_x = x;
            m_translate_y = y;
        }

        void setRotate(float radians) { m_rotate = radians; }
        void setScale(float scale) { m_scale = scale; }

    protected:
        struct SegmentInfo {
            size_t m_segment_index = 0;
            float m_local_t        = -1.0f;

            bool isValid() const { return m_local_t >= 0.0f && m_local_t <= 1.0f; }
        };

        SegmentInfo getSegmentForT(float t) const;
        float getRotationForT(float t) const;

        void compileControlPointsToPathPoints(size_t resolution);
        PathPoint computeInterpolatedPoint(float t) const;

    private:
        std::vector<PathPoint> m_path_points;
        std::vector<ControlPoint> m_control_points;

        float m_translate_x;
        float m_translate_y;
        float m_rotate;
        float m_scale;
    };

}  // namespace Toolbox