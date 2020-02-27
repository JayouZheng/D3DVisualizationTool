//
// FSceneDataImporter.h
//

#pragma once

#include "../AppData.h"

namespace UnrealEngine
{
	class FSceneDataImporter
	{
	public:

		FSceneDataImporter() {}

		void FillDataSets(const std::wstring& path);

		const FSceneDataSet* GetFSceneData(int lod) const;

		int GetLODCount() const { return (int)m_perLODDataSets.size(); }

		bool GetDataDirtyFlag() const { return bFSceneDataDirtyFlag; }

		void SetDataDirtyFlag(bool flag) { bFSceneDataDirtyFlag = flag; }

	private:

		void _FillDataSets(std::unordered_map <std::wstring, std::vector<std::vector<std::wstring>>>& clear_tables);

		std::vector<FSceneDataSet> m_perLODDataSets;

		bool bFSceneDataDirtyFlag = false;
	};

}