#pragma once

#include "core/threaded.hpp"

class RarcProcessor : public Toolbox::Threaded<void> {
protected:
    void tRun_(void *param) override;

protected:
    void requestCompileArchive(const fs_path &src_path, const fs_path &dest_path);
    void requestExtractArchive(const fs_path &arc_path, const fs_path &dest_path);

private:
    std::condition_variable m_arc_cv;
};