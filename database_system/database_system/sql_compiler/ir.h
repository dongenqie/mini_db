// =============================================
// sql_compiler/ir.h  ���� ��Ԫʽ IR������׶β�����
// =============================================
#pragma once
#include "../utils/common.h"
#include <string>
#include <vector>

namespace minidb {

    struct Quad {
        std::string op;
        std::string arg1;
        std::string arg2;
        std::string result;
    };

    struct IR {
        std::vector<Quad> quads;
        // ͳһ��ӡ��  [i] (op, arg1, arg2, result)
        void print(std::ostream& os) const {
            for (size_t i = 0; i < quads.size(); ++i) {
                const auto& q = quads[i];
                os << i << " (" << q.op << ", "
                    << q.arg1 << ", "
                    << q.arg2 << ", "
                    << q.result << ")\n";
            }
        }
    };

} // namespace minidb

