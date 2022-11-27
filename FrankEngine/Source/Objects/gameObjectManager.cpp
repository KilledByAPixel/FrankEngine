////////////////////////////////////////////////////////////////////////////////////////
/*
	Game Object Manager Class
	Copyright 2013 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#include "frankEngine.h"
#include "../objects/gameObject.h"
#include "../objects/gameObjectManager.h"

GameObjectManager g_objectManager;			// singleton that contains all game objects in the world

bool GameObjectManager::lockDeleteObjects = true;		// to prevent improperly deleting objects

typedef pair<GameObjectHandle, class GameObject*> GameObjectHashPair;

#ifdef DEBUG
ConsoleCommandSimple(bool, clearFreedBlocks, true);
#else
ConsoleCommandSimple(bool, clearFreedBlocks, false);
#endif //  DEBUG

////////////////////////////////////////////////////////////////////////////////////////
/*
	Game Object Manager
*/
////////////////////////////////////////////////////////////////////////////////////////

void GameObjectManager::Init( int _maxObjectCount, size_t _blockSize )
{
	ASSERT(!memPool);
	blockSize = _blockSize;
	maxObjectCount = _maxObjectCount;
	largestObjectSize = 0;
	memPool = (BYTE*)_aligned_malloc(maxObjectCount * blockSize, 16);
	for(int i = 0; i < maxObjectCount; ++i)
		freeObjectList.push_back(&memPool[i*blockSize]);
}

GameObjectManager::~GameObjectManager()
{
	ASSERT(memPool && GetObjectCount() == 0); // all objects should be removed by now
	RemoveAll(); 
	_aligned_free(memPool);
}

void* GameObjectManager::AllocateBlock(size_t size)
{
	ASSERT(size <= blockSize);
	ASSERT(!freeObjectList.empty());
	largestObjectSize = Max(size, largestObjectSize);

	if (freeObjectList.empty() || size > blockSize)
	{
		// ran out of blocks, use dynamic allocation
		++dynamicObjectCount;
		return (BYTE*)_aligned_malloc(size, 16);
	}

	void* block = freeObjectList.back();
	freeObjectList.pop_back();
	return block;
}

void GameObjectManager::FreeBlock(void* block)
{
	if (block < memPool || block >= memPool + maxObjectCount * blockSize)
	{
		// was not allocated from block, free dynamic
		ASSERT(dynamicObjectCount > 0);
		--dynamicObjectCount;
		_aligned_free(block); 
		return;
	}

	if (clearFreedBlocks)
	{
		// to help find dangling pointers, blocks can be cleared when freed
		for (size_t i = 0; i < blockSize; ++i)
			((BYTE*)(block))[i] = 0;
	}

	freeObjectList.push_back(block);
}

GameObject* GameObjectManager::GetObjectFromHandle(GameObjectHandle handle) 
{ 
	if (handle == 0)
		return NULL;

	// look for matching handle
	GameObjectHashTable::iterator it = objects.find(handle);
	if (it != objects.end())
	{
		GameObject* object = ((*it).second);
		return object;
	}
	else
		return NULL;
}

void GameObjectManager::Add(GameObject& obj)
{
	ASSERT(!GetObjectFromHandle(obj.GetHandle())); // handle not unique!
	objects.insert(GameObjectHashPair(obj.GetHandle(), &obj));
}

void GameObjectManager::Remove(const GameObject& obj) 
{
	ASSERT(GetObjectFromHandle(obj.GetHandle())); // make sure object is in table
	objects.erase(obj.GetHandle());
}

void GameObjectManager::Update()
{
	for (const GameObjectHashPair& hashPair : objects)
	{
		GameObject& obj = *hashPair.second;
		if (obj.IsDestroyed() || obj.WasJustAdded())
			continue;
		
		obj.Update();
	}
}

// call this once per frame to clear out dead objects
void GameObjectManager::UpdateTransforms()
{
	// clear render objects because the objects may be deleted
	sortedRenderObjects.clear();

	lockDeleteObjects = false;
	for (GameObjectHashTable::iterator it = objects.begin(); it != objects.end();)
	{
		GameObject& obj = *it->second;

		/*if (obj.wasJustAdded && obj.destroyThis)
		{
			g_debugMessageSystem.AddError( L"Object %d at (%0.2f, %0.2f) destroyed same frame as it was created.", 
				obj.GetHandle(), obj.GetXFWorld().position.x, obj.GetXFWorld().position.y );
		}*/
		
		if (obj.WasJustAdded())
			obj.SetFlag(GameObject::ObjectFlag_JustAdded, false);

		if (obj.IsDestroyed())
		{	
			ASSERT(!obj.parent && obj.children.empty());
			ASSERT(GetObjectFromHandle(obj.GetHandle())); // make sure object is in table
			it = objects.erase(it);
			delete &obj;
		} 
		else
		{
			if (!obj.HasParent())
				obj.UpdateTransforms();
			++it;
		}
	}
	lockDeleteObjects = true;
}

void GameObjectManager::SaveLastWorldTransforms()
{
	// save the last world transform for interpolation
	for (const GameObjectHashPair& hashPair : objects)
	{
		GameObject& obj = *hashPair.second;
		obj.xfWorldLast = obj.xfWorld;
	}
}

// sort objects by render order
bool GameObjectManager::RenderSortCompare(GameObject* first, GameObject* second)
{
	return (first->GetRenderGroup() < second->GetRenderGroup());
}

void GameObjectManager::CreateRenderList()
{
	sortedRenderObjects.clear();
	
	for (const GameObjectHashPair& hashPair : objects)
	{
		GameObject& obj = *hashPair.second;

		if (obj.IsVisible())
			sortedRenderObjects.push_back(&obj);
	}
	
	sortedRenderObjects.sort(RenderSortCompare);
}

void GameObjectManager::Render()
{
	int renderGroup = sortedRenderObjects.empty()? 0 : (*sortedRenderObjects.front()).GetRenderGroup();
	for (GameObject* obj : sortedRenderObjects )
	{
		if (obj->IsDestroyed())
			continue;

		if (renderGroup != obj->GetRenderGroup())
		{
			// always render simple verts and disable additive at the end of each group
			g_render->RenderSimpleVerts();
			g_render->SetSimpleVertsAreAdditive(false);
		}

		renderGroup = obj->GetRenderGroup();
		obj->Render();
	}

	g_render->RenderSimpleVerts();
	g_render->SetSimpleVertsAreAdditive(false);
}

void GameObjectManager::RenderPost()
{
	int renderGroup = sortedRenderObjects.empty()? 0 : (*sortedRenderObjects.front()).GetRenderGroup();
	for (GameObject* obj : sortedRenderObjects )
	{
		if (obj->IsDestroyed())
			continue;

		if (renderGroup != obj->GetRenderGroup())
		{
			// always render simple verts and disable additive at the end of each group
			g_render->RenderSimpleVerts();
			g_render->SetSimpleVertsAreAdditive(false);
		}

		renderGroup = obj->GetRenderGroup();

		obj->RenderPost();
	}

	g_render->RenderSimpleVerts();
	g_render->SetSimpleVertsAreAdditive(false);
}

void GameObjectManager::RemoveAll()
{
	// do a normal flush to get rid of all the game objects
	Reset();

	lockDeleteObjects = false;
	for (const GameObjectHashPair& hashPair : objects)
		delete hashPair.second;
	objects.clear();
	sortedRenderObjects.clear();
	lockDeleteObjects = true;
}

void GameObjectManager::Reset()
{
	// clear render objects because the objects may be deleted
	sortedRenderObjects.clear();

	// first mark for destroy
	for (GameObjectHashTable::iterator it = objects.begin(); it != objects.end(); ++it)
	{
		GameObject& obj = *it->second;

		if (obj.DestroyOnWorldReset())
		{
			ASSERT(!obj.HasParent() || obj.GetParent()->DestroyOnWorldReset());
			obj.Destroy();
		}
	}

	// go through the list again and delete stuff
	lockDeleteObjects = false;
	for (GameObjectHashTable::iterator it = objects.begin(); it != objects.end();)
	{
		GameObject& obj = *it->second;
		if (obj.IsDestroyed())
		{	
			ASSERT(!obj.parent && obj.children.empty());
			it = objects.erase(it);
			delete &obj;
		} 
		else
			++it;
	}
	lockDeleteObjects = true;
}


list<GameObject*> GameObjectManager::GetObjects(const Vector2& pos, float radius, bool skipChildern)
{
	list<GameObject*> results;

	const float radius2 = Square(radius);
	for (const GameObjectHashPair& hashPair : objects)
	{
		GameObject& obj = *hashPair.second;

		if (obj.WasJustAdded() || skipChildern && obj.GetParent())
			continue;

		if ((pos - obj.GetPosWorld()).LengthSquared() < radius2)
			results.push_back(&obj);
	}

	return results;
}