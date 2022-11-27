////////////////////////////////////////////////////////////////////////////////////////
/*
	Game Object Manager Class
	Copyright 2013 Frank Force - http://www.frankforce.com

	- hash map of game objects using their unique handle as the hash
	- handles update and render of objects
	- protects against improper deleting of objects
*/
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <unordered_map>

// global game object manager singleton
extern class GameObjectManager g_objectManager;

class GameObject;
typedef unsigned GameObjectHandle;
typedef pair<GameObjectHandle, class GameObject*> GameObjectHashPair;
typedef unordered_map<GameObjectHandle, class GameObject*> GameObjectHashTable;

class GameObjectManager
{
public:

	void Init( int maxObjectCount, size_t blockSize );
	~GameObjectManager();

public:	// call these functions once per frame

	void Update();
	void SaveLastWorldTransforms();
	void CreateRenderList();
	void Render();
	void RenderPost();

public:	// functions to get / add / remove objects

	void Add(GameObject& obj);
	void Remove(const GameObject& obj);
	void RemoveAll();
	void Reset();
	void UpdateTransforms();

	GameObjectHashTable& GetObjects() { return objects; }
	list<GameObject*> GetObjects(const Vector2& pos, float radius, bool skipChildern);
	GameObject* GetObjectFromHandle(GameObjectHandle handle);
	int GetObjectCount() const { return objects.size(); }
	int GetDynamicObjectCount() const { return dynamicObjectCount; }
	int GetLargestObjectSize() const { return largestObjectSize; }
	int GetBlockSize() const { return blockSize; }

	static bool GetLockDeleteObjects() { return lockDeleteObjects; }
	
	void* AllocateBlock(size_t size);
	void FreeBlock(void* block);

private:

	static bool RenderSortCompare(GameObject* first, GameObject* second);

	GameObjectHashTable objects;					// hash map of all objects
	list<GameObject *> sortedRenderObjects;			// list of objects to render sorted by render group
	static bool lockDeleteObjects;					// to prevent improperly deleting objects
	
	BYTE* memPool = NULL;							// use block allocator for objects
	list<void*> freeObjectList;						// keep list of free blocks
	int maxObjectCount = 0;							// max number of objects that can exist
	size_t blockSize = 0;							// block size must be large enough for largest object
	size_t largestObjectSize = 0;					// track the largest object
	int dynamicObjectCount = 0;						// track how many objects used dynamic allocation
};
