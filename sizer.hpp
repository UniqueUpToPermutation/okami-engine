#pragma once

#include <optional>
#include <algorithm>

namespace okami {
    struct Sizer {
	public:
		double m_weightedSize = 0.0;
		double m_sizeDecay = 0.95; // Decay factor for size
		double m_expandFactor = 2.0; // Factor to expand size when needed
		size_t m_currentSize = 0;
		size_t m_minSize = 0;

		inline size_t Reset(size_t m_size) {
			m_currentSize = std::max<size_t>(m_size, m_minSize);
			m_weightedSize = static_cast<double>(m_currentSize);
			return m_currentSize;
		}

		inline std::optional<size_t> GetNextSize(size_t requestedSize) {
			m_weightedSize = (1 - m_sizeDecay) * static_cast<double>(requestedSize) + m_weightedSize * m_sizeDecay;

			if (requestedSize >= m_currentSize) {
				// Buffer is not large enough, expand it
				return Reset(static_cast<size_t>(m_weightedSize * m_expandFactor));
			}
			else if (m_weightedSize <= m_currentSize / (m_expandFactor * m_expandFactor)) {
				if (m_currentSize > m_minSize)
				{
					// Buffer is too large, shrink it
					return Reset(static_cast<size_t>(m_weightedSize * m_expandFactor));
				} else {
					// Can't shrink below minimum size	
					return {};
				}
			}
			else {
				return {};
			}
		}

		inline double operator()() const {
			return m_weightedSize;
		}
	};
} // namespace okami