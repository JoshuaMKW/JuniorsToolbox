#include "spline.hpp"
#include "core/log.hpp"

#include <algorithm>
#include <cmath>

namespace Toolbox {

    CatmullSpline2D::CatmullSpline2D(const std::vector<ControlPoint> &control_points,
                                     size_t resolution)
        : m_control_points(control_points), m_translate_x(0.0f), m_translate_y(0.0f),
          m_rotate(0.0f), m_scale(1.0f) {
        TOOLBOX_CORE_ASSERT(control_points.size() >= 4);
        compileControlPointsToPathPoints(resolution);
    }

    float CatmullSpline2D::getLength() const {
        if (m_path_points.size() < 2) {
            return 0.0f;
        }

        float total_length = 0.0f;

        for (size_t i = 0; i < m_path_points.size() - 1; ++i) {
            const auto &p1 = m_path_points[i];
            const auto &p2 = m_path_points[i + 1];

            float dx = (p2.m_x - p1.m_x) * m_scale;
            float dy = (p2.m_y - p1.m_y) * m_scale;

            total_length += std::sqrt((dx * dx) + (dy * dy));
        }

        return total_length;
    }

    CatmullSpline2D::PathPoint CatmullSpline2D::getInterpolatedPoint(float t) const {
        if (m_path_points.empty()) {
            return PathPoint{};
        }

        t                 = std::clamp(t, 0.0f, 1.0f);
        float exact_index = t * static_cast<float>(m_path_points.size() - 1);
        size_t index0     = static_cast<size_t>(exact_index);

        if (index0 >= m_path_points.size() - 1) {
            return m_path_points.back();
        }

        size_t index1      = index0 + 1;
        float fractional_t = exact_index - static_cast<float>(index0);

        const auto &p0 = m_path_points[index0];
        const auto &p1 = m_path_points[index1];

        PathPoint result;
        const float p_x = (p0.m_x + (p1.m_x - p0.m_x) * fractional_t) * m_scale;
        const float p_y = (p0.m_y + (p1.m_y - p0.m_y) * fractional_t) * m_scale;
        result.m_x = m_translate_x + p_x * std::cosf(m_rotate) - p_y * std::sinf(m_rotate);
        result.m_y = m_translate_y + p_x * std::sinf(m_rotate) + p_y * std::cosf(m_rotate);

        float rot_diff = p1.m_tan_rotation - p0.m_tan_rotation;

        constexpr float PI = 3.14159265359f;
        while (rot_diff < -PI)
            rot_diff += 2.0f * PI;
        while (rot_diff > PI)
            rot_diff -= 2.0f * PI;

        result.m_tan_rotation = m_rotate + p0.m_tan_rotation + (rot_diff * fractional_t);

        if (result.m_tan_rotation < 0.0f) {
            result.m_tan_rotation += 2.0f * PI;
        } else if (result.m_tan_rotation >= 2.0f * PI) {
            result.m_tan_rotation -= 2.0f * PI;
        }

        return result;
    }

    CatmullSpline2D::PathPoint CatmullSpline2D::getDiscretePoint(size_t index) const {
        if (m_path_points.empty()) {
            return PathPoint{};
        }

        index           = std::min(index, m_path_points.size() - 1);
        PathPoint point = m_path_points[index];

        const float p_x = point.m_x * m_scale;
        const float p_y = point.m_y * m_scale;

        point.m_x      = m_translate_x + p_x * std::cosf(m_rotate) - p_y * std::sinf(m_rotate);
        point.m_y      = m_translate_y + p_x * std::sinf(m_rotate) + p_y * std::cosf(m_rotate);

        point.m_tan_rotation += m_rotate;
    }

    CatmullSpline2D::SegmentInfo CatmullSpline2D::getSegmentForT(float t) const {
        const size_t segment_count = m_control_points.size() - 3;

        if (segment_count == 0) {
            return SegmentInfo{0, 0.0f};
        }

        t = std::clamp(t, 0.0f, 1.0f);

        // Scale global t up to the local segment coordinate space
        const float scaled_t = t * static_cast<float>(segment_count);
        size_t segment_index = static_cast<size_t>(scaled_t);

        // Handle the absolute end of the spline edge case (t == 1.0f)
        if (segment_index >= segment_count) {
            segment_index = segment_count - 1;
            return SegmentInfo{segment_index, 1.0f};
        }

        const float local_t = scaled_t - static_cast<float>(segment_index);
        return SegmentInfo{segment_index, local_t};
    }

    float CatmullSpline2D::getRotationForT(float t) const {
        PathPoint current_pos = getInterpolatedPoint(t);

        float epsilon = 0.001f;
        float next_t  = std::min(1.0f, t + epsilon);

        if (t >= 1.0f) {
            current_pos = getInterpolatedPoint(t - epsilon);
            next_t      = t;
        }

        PathPoint next_pos = getInterpolatedPoint(next_t);

        float dx = next_pos.m_x - current_pos.m_x;
        float dy = next_pos.m_y - current_pos.m_y;

        float angle_radians = std::atan2(dy, dx);

        constexpr float PI = 3.14159265359f;
        if (angle_radians < 0.0f) {
            angle_radians += 2 * PI;
        }

        return angle_radians;
    }

    float CatmullSpline2D::getInterpolationFromPoint(const PathPoint &point) const {
        // Reverse-mapping a 2D coordinate to 't' requires nearest-neighbor
        // distance checks against the compiled discrete points.
        if (m_path_points.empty()) {
            return 0.0f;
        }

        float min_dist_sq    = std::numeric_limits<float>::max();
        size_t closest_index = 0;

        for (size_t i = 0; i < m_path_points.size(); ++i) {
            const auto &p = m_path_points[i];
            float dx      = (m_translate_x + (p.m_x * m_scale)) - point.m_x;
            float dy      = (m_translate_y + (p.m_y * m_scale)) - point.m_y;
            float dist_sq = (dx * dx) + (dy * dy);

            if (dist_sq < min_dist_sq) {
                min_dist_sq   = dist_sq;
                closest_index = i;
            }
        }

        return static_cast<float>(closest_index) / static_cast<float>(m_path_points.size() - 1);
    }

    float CatmullSpline2D::getInterpolationFromLength(float length) const {
        if (m_path_points.size() < 2 || length <= 0.0f) {
            return 0.0f;
        }

        float current_length = 0.0f;

        for (size_t i = 0; i < m_path_points.size() - 1; ++i) {
            const auto &p1 = m_path_points[i];
            const auto &p2 = m_path_points[i + 1];

            float dx             = (p2.m_x - p1.m_x) * m_scale;
            float dy             = (p2.m_y - p1.m_y) * m_scale;
            float segment_length = std::sqrt((dx * dx) + (dy * dy));

            if (current_length + segment_length >= length) {
                float remainder    = length - current_length;
                float fractional_t = remainder / segment_length;

                // Map the local segment fraction back to the global 0.0 -> 1.0 t-space
                return (static_cast<float>(i) + fractional_t) /
                       static_cast<float>(m_path_points.size() - 1);
            }

            current_length += segment_length;
        }

        return 1.0f;
    }

    float CatmullSpline2D::getLengthFromInterpolation(float t) const {
        if (m_path_points.size() < 2 || t <= 0.0f) {
            return 0.0f;
        }

        if (t > 1.0f) {
            t = 1.0f;
        }

        float exact_index      = t * static_cast<float>(m_path_points.size() - 1);
        size_t last_full_index = static_cast<size_t>(exact_index);

        if (last_full_index >= m_path_points.size() - 1) {
            last_full_index = m_path_points.size() - 2;
            exact_index     = static_cast<float>(last_full_index + 1);
        }

        float fractional_t = exact_index - static_cast<float>(last_full_index);

        float current_length = 0.0f;

        for (size_t i = 0; i < last_full_index; ++i) {
            const auto &p1 = m_path_points[i];
            const auto &p2 = m_path_points[i + 1];

            float dx = (p2.m_x - p1.m_x) * m_scale;
            float dy = (p2.m_y - p1.m_y) * m_scale;
            current_length += std::sqrt((dx * dx) + (dy * dy));
        }

        const auto &p1 = m_path_points[last_full_index];
        const auto &p2 = m_path_points[last_full_index + 1];

        float dx                   = (p2.m_x - p1.m_x) * m_scale;
        float dy                   = (p2.m_y - p1.m_y) * m_scale;
        float final_segment_length = std::sqrt((dx * dx) + (dy * dy));

        current_length += final_segment_length * fractional_t;

        return current_length;
    }

    void CatmullSpline2D::compileControlPointsToPathPoints(size_t resolution) {
        m_path_points.clear();

        if (m_control_points.size() < 4 || resolution == 0) {
            return;
        }

        const size_t segment_count = m_control_points.size() - 3;

        m_path_points.reserve(segment_count * resolution);

        for (size_t i = 0; i < segment_count; ++i) {
            for (size_t j = 0; j < resolution; ++j) {
                float local_t = static_cast<float>(j) / static_cast<float>(resolution);
                float global_t =
                    (static_cast<float>(i) + local_t) / static_cast<float>(segment_count);

                m_path_points.push_back(computeInterpolatedPoint(global_t));
            }
        }

        // Push the final point capping the end of the curve
        m_path_points.push_back(computeInterpolatedPoint(1.0f));

        for (size_t i = 0; i < segment_count; ++i) {
            for (size_t j = 0; j < resolution; ++j) {
                float local_t = static_cast<float>(j) / static_cast<float>(resolution);
                float global_t =
                    (static_cast<float>(i) + local_t) / static_cast<float>(segment_count);

                m_path_points[i * resolution + j].m_tan_rotation = getRotationForT(global_t);
            }
        }

        m_path_points.back().m_tan_rotation = getRotationForT(1.0f);
    }

    CatmullSpline2D::PathPoint CatmullSpline2D::computeInterpolatedPoint(float t) const {
        if (m_control_points.size() < 4) {
            return PathPoint{};
        }

        SegmentInfo info = getSegmentForT(t);

        const auto &p0 = m_control_points[info.m_segment_index];
        const auto &p1 = m_control_points[info.m_segment_index + 1];
        const auto &p2 = m_control_points[info.m_segment_index + 2];
        const auto &p3 = m_control_points[info.m_segment_index + 3];

        float lt  = info.m_local_t;
        float lt2 = lt * lt;
        float lt3 = lt2 * lt;

        PathPoint result;

        result.m_x =
            0.5f * ((2.0f * p1.first) + (-p0.first + p2.first) * lt +
                    (2.0f * p0.first - 5.0f * p1.first + 4.0f * p2.first - p3.first) * lt2 +
                    (-p0.first + 3.0f * p1.first - 3.0f * p2.first + p3.first) * lt3);

        result.m_y =
            0.5f * ((2.0f * p1.second) + (-p0.second + p2.second) * lt +
                    (2.0f * p0.second - 5.0f * p1.second + 4.0f * p2.second - p3.second) * lt2 +
                    (-p0.second + 3.0f * p1.second - 3.0f * p2.second + p3.second) * lt3);

        return result;
    }

}  // namespace Toolbox
