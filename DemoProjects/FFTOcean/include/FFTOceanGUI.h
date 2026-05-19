#pragma once

namespace EduEngine
{
	class FFTOceanDemo;

	class FFTOceanGUI
	{
	public:
		void Init(FFTOceanDemo* parent);
		void RenderImGUI();
	
	private:
		FFTOceanDemo* m_FFTOceanDemo;
	};
}