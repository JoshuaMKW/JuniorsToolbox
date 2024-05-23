from svg.path import parse_path
from svg.path.path import Line, CubicBezier, QuadraticBezier, Arc
import xml.etree.ElementTree as ET

# Load the SVG file
svg_file_path = "gamecube_base.svg"
tree = ET.parse(svg_file_path)
root = tree.getroot()

# Extract the path data from the SVG file
svg_path_data = []
for element in root.iter("{http://www.w3.org/2000/svg}path"):
    svg_path_data.append(element.attrib["d"])

print(svg_path_data)

# Parse the path data
all_points = []
for path_data in svg_path_data:
    path = parse_path(path_data)
    for segment in path:
        if isinstance(segment, Line):
            all_points.append((segment.start.real, segment.start.imag))
            all_points.append((segment.end.real, segment.end.imag))
        elif isinstance(segment, (CubicBezier, QuadraticBezier, Arc)):
            # For more complex segments, sample points along the path
            num_samples = 10
            for i in range(num_samples + 1):
                point = segment.point(i / num_samples)
                all_points.append((point.real, point.imag))

# Remove duplicate points and round to the desired precision
unique_points = list(set((round(x, 4), round(y, 4)) for x, y in all_points))
unique_points.sort()


def points_to_cpp_vec(points, var_name):
    cpp_out = f"static const std::vector<ImVec2> {var_name} = " + "{\n"
    for point in points:
        cpp_out += "{" + str(point[0]) + "," + str(point[1]) + "},"
    cpp_out += "\n};"
    return cpp_out


print(points_to_cpp_vec(unique_points, "s_controller_base"))
