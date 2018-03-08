#pragma once
#include <map>
#include <vector>
#include "CriticalSection.h"

template <typename T>
class Container
{
protected:
	typedef std::map<int, T*> MAP_CONTANINER;
	typedef std::vector<T*> VEC_CONTANINER;

public:
	void MapPopBack(int key);
	T* VecPopBack();

	void MapPushBack(int id, T* value);
	void VecPushBack(T* value);


public:
	Container();
	virtual ~Container();

protected:
	MAP_CONTANINER m_mapCon;
	VEC_CONTANINER m_vecCon;

	SCriticalSection m_cs;
};

template<typename T>
Container<T>::Container()
{
}

template<typename T>
Container<T>::~Container()
{
}

template<typename T>
void Container<T>::MapPopBack(int key)
{
	CSLOCK(m_cs)
	{
		MAP_CONTANINER::iterator it = m_mapCon.fine(key);
		if (it != m_mapCon.end())
		{
			m_mapCon.erase(it);
		}
	}
}

template<typename T>
T * Container<T>::VecPopBack()
{
	if (m_vecCon.size() == 0)
	{
		//UnLock();
		return NULL;
	}

	T* value = NULL;

	CSLOCK(m_cs)
	{
		VEC_CONTANINER::iterator it = m_vecCon.begin();

		value = *it;
		m_vecCon.erase(it);
	}

	return value;
}

template<typename T>
void Container<T>::MapPushBack(int id, T * value)
{
	if (puser == NULL)
		return;

	CSLOCK(m_cs)
	{
		m_mapCon.insert(std::pair<int, SPeer*>(userid, puser));
	}

	return;
}


template<typename T>
void Container<T>::VecPushBack(T * value)
{
	if (value == NULL) return;

	CSLOCK(m_cs)
	{
		VEC_CONTANINER::iterator it = std::find(m_vecCon.begin(), m_vecCon.end(), value);

		if (it != m_vecCon.end())
			return;

		m_vecCon.push_back(value);
	}
}