/*=auto=========================================================================

Portions (c) Copyright 2005 Brigham and Women's Hospital (BWH) All Rights Reserved.

See COPYRIGHT.txt
or http://www.slicer.org/copyright/copyright.txt for details.

Program:   3D Slicer
Module:    $RCSfile: vtkMRMLSceneViewNode.cxx,v $
Date:      $Date: 2006/03/17 17:01:53 $
Version:   $Revision: 1.14 $

=========================================================================auto=*/

// MRML includes
#include "vtkMRMLHierarchyNode.h"
#include "vtkMRMLScene.h"
#include "vtkMRMLSceneViewNode.h"
#include "vtkMRMLSceneViewStorageNode.h"

// VTKsys includes
#include <vtksys/SystemTools.hxx>

// VTK includes
#include <vtkCollection.h>
#include <vtkImageData.h>
#include <vtkObjectFactory.h>
#include <vtkSmartPointer.h>

// STD includes
#include <cassert>
#include <sstream>
#include <stack>

//----------------------------------------------------------------------------
vtkMRMLNodeNewMacro(vtkMRMLSceneViewNode);

//----------------------------------------------------------------------------
vtkMRMLSceneViewNode::vtkMRMLSceneViewNode()
{
  this->HideFromEditors = 0;

  this->Nodes = NULL;
//  this->ScreenShot = vtkImageData::New();
  this->ScreenShot = NULL;
  this->ScreenShotType = 0;
}

//----------------------------------------------------------------------------
vtkMRMLSceneViewNode::~vtkMRMLSceneViewNode()
{
  if (this->Nodes)
    {
    this->Nodes->Delete();
    this->Nodes = 0;
    }
  if (this->ScreenShot)
    {
    this->ScreenShot->Delete();
    this->ScreenShot = NULL;
    }
}

//----------------------------------------------------------------------------
void vtkMRMLSceneViewNode::WriteXML(ostream& of, int nIndent)
{
  Superclass::WriteXML(of, nIndent);

  vtkIndent indent(nIndent);

  of << indent << " screenshotType=\"" << this->GetScreenShotType() << "\"";

  vtkStdString description = this->GetSceneViewDescription();
  vtksys::SystemTools::ReplaceString(description,"\n","[br]");

  of << indent << " sceneViewDescription=\"" << description << "\"";
}

//----------------------------------------------------------------------------
void vtkMRMLSceneViewNode::WriteNodeBodyXML(ostream& of, int nIndent)
{
  this->SetAbsentStorageFileNames();

  vtkMRMLNode * node = NULL;
  int n;
  for (n=0; n < this->Nodes->GetNodes()->GetNumberOfItems(); n++) 
    {
    node = (vtkMRMLNode*)this->Nodes->GetNodes()->GetItemAsObject(n);
    if (node && !node->IsA("vtkMRMLSceneViewNode") && node->GetSaveWithScene())
      {
      vtkIndent vindent(nIndent+1);
      of << vindent << "<" << node->GetNodeTagName() << "\n";

      node->WriteXML(of, nIndent + 2);

      of << vindent << ">";
      node->WriteNodeBodyXML(of, nIndent+1);
      of << "</" << node->GetNodeTagName() << ">\n";
      }
    }
    
}

//----------------------------------------------------------------------------
void vtkMRMLSceneViewNode::ReadXMLAttributes(const char** atts)
{

  int disabledModify = this->StartModify();

  Superclass::ReadXMLAttributes(atts);

  const char* attName;
  const char* attValue;
  while (*atts != NULL)
    {
    attName = *(atts++);
    attValue = *(atts++);
    if (!strcmp(attName, "screenshotType"))
      {
      std::stringstream ss;
      ss << attValue;
      int screenshotType;
      ss >> screenshotType;
      this->SetScreenShotType(screenshotType);
      }
    else if(!strcmp(attName, "sceneViewDescription"))
      {
      // can have spaces in the description, don't use stringstream
      vtkStdString sceneViewDescription = vtkStdString(attValue);
      vtksys::SystemTools::ReplaceString(sceneViewDescription,"[br]","\n");
      this->SetSceneViewDescription(sceneViewDescription);
      }
    }

  // for backward compatibility:
  
  // now read the screenCapture if there's a directory for them
  // TODO: don't do this if there is a storage node already, but the problem
  // is that the storage node will get set after, so GetStorageNode returns
  // null right now
  vtkStdString screenCapturePath;
  if (this->GetScene() &&
      this->GetScene()->GetRootDirectory())
    {
    screenCapturePath += this->GetScene()->GetRootDirectory();
    }
  else
    {
    screenCapturePath += ".";
    }
  screenCapturePath += "/";
  screenCapturePath += "ScreenCaptures/";
  
  vtkStdString screenCaptureFilename;
  screenCaptureFilename += screenCapturePath;
  if (this->GetID())
    {
    screenCaptureFilename += this->GetID();
    }
  else
    {
    screenCaptureFilename += "vtkMRMLSceneViewNodeNoID";
    }
  screenCaptureFilename += ".png";
  
  
  if (vtksys::SystemTools::FileExists(vtksys::SystemTools::ConvertToOutputPath(screenCaptureFilename.c_str()).c_str(),true))
    {
    // create a storage node and use it to read the file
    vtkMRMLStorageNode *storageNode = this->GetStorageNode();
    if (storageNode == NULL)
      {
      // only read the directory if there isn't a storage node already
      storageNode = this->CreateDefaultStorageNode();
      if (storageNode)
        {
        storageNode->SetFileName(vtksys::SystemTools::ConvertToOutputPath(screenCaptureFilename.c_str()).c_str());
        if (this->GetScene())
          {
          this->GetScene()->AddNode(storageNode);
          }
        vtkWarningMacro("ReadXMLAttributes: found the ScreenCapture directory, creating a storage node to read the image file at\n\t" << storageNode->GetFileName() << "\n\tImage data be overwritten if there is a storage node pointing to another file");
        storageNode->ReadData(this);
        storageNode->Delete();
        }
      }
    else
      {
      vtkWarningMacro("ReadXMLAttributes: there is a ScreenCaptures directory with a valid file in it, but waiting to let the extant storage node read it's image file");
      }
    }

  this->EndModify(disabledModify);
}

//----------------------------------------------------------------------------
void vtkMRMLSceneViewNode::ProcessChildNode(vtkMRMLNode *node)
{
  // for the child nodes in the scene view scene, we don't want to invoke any
  // pending modified events when done processing them, so just use the bare
  // DisableModifiedEventOn and SetDisableModifiedEvent calls rather than
  // using StartModify and EndModify
  int disabledModify = this->GetDisableModifiedEvent();
  this->DisableModifiedEventOn();
    
  int disabledModifyNode = node->GetDisableModifiedEvent();
  node->DisableModifiedEventOn();

  Superclass::ProcessChildNode(node);
  node->SetAddToSceneNoModify(0);

  if (this->Nodes == NULL)
    {
    this->Nodes = vtkMRMLScene::New();
    }  
  this->Nodes->GetNodes()->vtkCollection::AddItem((vtkObject *)node);

  this->Nodes->AddNodeID(node);

  node->SetScene(this->Nodes);

  node->SetDisableModifiedEvent(disabledModifyNode);
  this->SetDisableModifiedEvent(disabledModify);

}

//----------------------------------------------------------------------------
// Copy the node's attributes to this object.
// Does NOT copy: ID, FilePrefix, Name, VolumeID
void vtkMRMLSceneViewNode::Copy(vtkMRMLNode *anode)
{
  Superclass::Copy(anode);
  vtkMRMLSceneViewNode *snode = (vtkMRMLSceneViewNode *) anode;

  this->SetScreenShot(vtkMRMLSceneViewNode::SafeDownCast(anode)->GetScreenShot());
  this->SetScreenShotType(vtkMRMLSceneViewNode::SafeDownCast(anode)->GetScreenShotType());
  this->SetSceneViewDescription(vtkMRMLSceneViewNode::SafeDownCast(anode)->GetSceneViewDescription());

  if (this->Nodes == NULL)
    {
    this->Nodes = vtkMRMLScene::New();
    }
  else
    {
    this->Nodes->GetNodes()->RemoveAllItems();
    this->Nodes->ClearNodeIDs();
    }
  vtkMRMLNode *node = NULL;
  if ( snode->Nodes != NULL )
    {
    int n;
    for (n=0; n < snode->Nodes->GetNodes()->GetNumberOfItems(); n++) 
      {
      node = (vtkMRMLNode*)snode->Nodes->GetNodes()->GetItemAsObject(n);
      if (node)
        {
        node->SetScene(this->Nodes);
        this->Nodes->AddNodeID(node);
       }
      }
    }
}

//----------------------------------------------------------------------------
void vtkMRMLSceneViewNode::PrintSelf(ostream& os, vtkIndent indent)
{
  Superclass::PrintSelf(os,indent);
}

//----------------------------------------------------------------------------
void vtkMRMLSceneViewNode::UpdateScene(vtkMRMLScene *scene)
{
  // the superclass update scene ensures that the storage node is read into
  // this storable node
  Superclass::UpdateScene(scene);
  if (!scene)
    {
    return;
    }
  if (this->Nodes)
    {
    this->Nodes->CopyNodeReferences(scene);
    this->Nodes->UpdateNodeChangedIDs();
    this->Nodes->UpdateNodeReferences();
    }
  this->UpdateSnapshotScene(this->Nodes);
}
//----------------------------------------------------------------------------

void vtkMRMLSceneViewNode::UpdateSnapshotScene(vtkMRMLScene *)
{
  if (this->Scene == NULL)
    {
    return;
    }

  if (this->Nodes == NULL)
    {
    return;
    }

  unsigned int nnodesSanpshot = this->Nodes->GetNodes()->GetNumberOfItems();
  unsigned int n;
  vtkMRMLNode *node = NULL;

  // prevent data read in UpdateScene
  for (n=0; n<nnodesSanpshot; n++) 
    {
    node  = dynamic_cast < vtkMRMLNode *>(this->Nodes->GetNodes()->GetItemAsObject(n));
    if (node) 
      {
      node->SetAddToSceneNoModify(0);
      }
    }

  // update nodes in the snapshot
  for (n=0; n<nnodesSanpshot; n++) 
    {
    node  = dynamic_cast < vtkMRMLNode *>(this->Nodes->GetNodes()->GetItemAsObject(n));
    if (node) 
      {
      node->UpdateScene(this->Nodes);
      }
    }

  /**
  // update nodes in the snapshot
  for (n=0; n<nnodesSanpshot; n++) 
    {
    node  = dynamic_cast < vtkMRMLNode *>(this->Nodes->GetNodes()->GetItemAsObject(n));
    if (node)
      {
      node->SetAddToSceneNoModify(1);
      }
    }
    ***/
}

//----------------------------------------------------------------------------
void vtkMRMLSceneViewNode::StoreScene()
{
  if (this->Scene == NULL)
    {
    return;
    }

  if (this->Nodes == NULL)
    {
    this->Nodes = vtkMRMLScene::New();
    }
  else
    {
    this->Nodes->Clear(1);
    }

  if (this->GetScene())
    {
    this->Nodes->SetRootDirectory(this->GetScene()->GetRootDirectory());
    }

  /// \todo: GetNumberOfNodes/GetNthNode is slow, fasten by using collection
  /// iterators.
  for (int n=0; n < this->Scene->GetNumberOfNodes(); n++) 
    {
    vtkMRMLNode *node = this->Scene->GetNthNode(n);
    if (this->IncludeNodeInSceneView(node) &&
        node->GetSaveWithScene() )
      {
      vtkMRMLNode *newNode = node->CreateNodeInstance();

      newNode->SetScene(this->Nodes);
      newNode->CopyWithoutModifiedEvent(node);
      newNode->SetID(node->GetID());

      newNode->SetAddToSceneNoModify(1);
      this->Nodes->AddNode(newNode);
      newNode->SetAddToSceneNoModify(0);

      // Node has been added into the scene, decrease reference count to 1.
      newNode->Delete();

      // sanity check
      assert(newNode->GetScene() == this->Nodes);
      }
    }
  this->Nodes->CopyNodeReferences(this->GetScene());
}

//----------------------------------------------------------------------------
void vtkMRMLSceneViewNode::RestoreScene()
{
  if (this->Scene == NULL)
    {
    vtkWarningMacro("No scene to restore onto");
    return;
    }
  if (this->Nodes == NULL)
    {
    vtkWarningMacro("No nodes to restore");
    return;
    }

  unsigned int numNodesInSceneView = this->Nodes->GetNodes()->GetNumberOfItems();
  unsigned int n;
  vtkMRMLNode *node = NULL;

  this->Scene->StartState(vtkMRMLScene::RestoreState);

  // remove nodes in the scene which are not stored in the snapshot
  std::map<std::string, vtkMRMLNode*> snapshotMap;
  for (n=0; n<numNodesInSceneView; n++) 
    {
    node  = dynamic_cast < vtkMRMLNode *>(this->Nodes->GetNodes()->GetItemAsObject(n));
    if (node) 
      {
      /***
      const char *newID = this->Scene->GetChangedID(node->GetID());
      if (newID)
        {
        snapshotMap[newID] = node;
        }
      else
        {
        snapshotMap[node->GetID()] = node;
        }
      ***/
      if (node->GetID()) 
        {
        snapshotMap[node->GetID()] = node;
        }
      }
    }
  // Identify which nodes must be removed from the scene.
  vtkCollectionSimpleIterator it;
  vtkCollection* sceneNodes = this->Scene->GetNodes();
  // Use smart pointer to ensure the nodes still exist when being removed.
  // Indeed, removing a node can have the side effect of removing other nodes.
  std::stack<vtkSmartPointer<vtkMRMLNode> > removedNodes;
  for (sceneNodes->InitTraversal(it);
       (node = vtkMRMLNode::SafeDownCast(sceneNodes->GetNextItemAsObject(it))) ;)
    {
    std::map<std::string, vtkMRMLNode*>::iterator iter = snapshotMap.find(std::string(node->GetID()));
    vtkSmartPointer<vtkMRMLHierarchyNode> hnode = vtkMRMLHierarchyNode::SafeDownCast(node);
    // don't remove the scene view nodes, the snapshot clip nodes, hierarchy nodes associated with the
    // sceneview nodes nor top level scene view hierarchy nodes
    if (iter == snapshotMap.end() &&
        this->IncludeNodeInSceneView(node) &&
        node->GetSaveWithScene())
      {
      removedNodes.push(vtkSmartPointer<vtkMRMLNode>(node));
      }
    }
  while(!removedNodes.empty())
    {
    vtkMRMLNode* nodeToRemove = removedNodes.top().GetPointer();
    // Remove the node only if it's not part of the scene.
    bool isNodeInScene = (nodeToRemove->GetScene() == this->Scene);
    // Decrease reference count before removing it from the scene
    // to give the opportunity of the node to be deleted in RemoveNode
    // (standard behavior).
    removedNodes.pop();
    if (isNodeInScene)
      {
      this->Scene->RemoveNode(nodeToRemove);
      }
    }

  std::vector<vtkMRMLNode *> addedNodes;
  for (n=0; n < numNodesInSceneView; n++) 
    {
    node = vtkMRMLNode::SafeDownCast(this->Nodes->GetNodes()->GetItemAsObject(n));
    if (node)
      {
      // don't restore certain nodes that might have been in the scene view by mistake
      if (this->IncludeNodeInSceneView(node))
        {
        vtkMRMLNode *snode = this->Scene->GetNodeByID(node->GetID());

        if (snode)
          {
          snode->SetScene(this->Scene);
          // to prevent copying of default info if not stored in sanpshot
          snode->CopyWithSingleModifiedEvent(node);
          // to prevent reading data on UpdateScene()
          snode->SetAddToSceneNoModify(0);
          }
        else 
          {
          vtkMRMLNode *newNode = node->CreateNodeInstance();
          newNode->CopyWithSceneWithoutModifiedEvent(node);
          
          addedNodes.push_back(newNode);
          newNode->SetAddToSceneNoModify(1);
          this->Scene->AddNode(newNode);
          newNode->Delete();
          
          // to prevent reading data on UpdateScene()
          // but new nodes should read their data
          //node->SetAddToSceneNoModify(0);
          }
        }
      }
    }
  
  // update all nodes in the scene

  //this->Scene->UpdateNodeReferences(this->Nodes);

  for (sceneNodes->InitTraversal(it);
       (node = vtkMRMLNode::SafeDownCast(sceneNodes->GetNextItemAsObject(it))) ;)
    {
    if (this->IncludeNodeInSceneView(node) && node->GetSaveWithScene())
      {
      node->UpdateScene(this->Scene);
      }
    }

  //this->Scene->SetIsClosing(0);
  for(n=0; n<addedNodes.size(); n++)
    {
    //addedNodes[n]->UpdateScene(this->Scene);
    //this->Scene->InvokeEvent(vtkMRMLScene::NodeAddedEvent, addedNodes[n] );
    }

  this->Scene->EndState(vtkMRMLScene::RestoreState);

#ifndef NDEBUG
  // sanity checks
  for (sceneNodes->InitTraversal(it);
       (node = vtkMRMLNode::SafeDownCast(sceneNodes->GetNextItemAsObject(it))) ;)
    {
    assert(node->GetScene() == this->Scene);
    }
#endif
}

//----------------------------------------------------------------------------
void vtkMRMLSceneViewNode::SetAbsentStorageFileNames()
{
  if (this->Scene == NULL)
    {
    return;
    }

  if (this->Nodes == NULL)
    {
    return;
    }

  unsigned int numNodesInSceneView = this->Nodes->GetNodes()->GetNumberOfItems();
  unsigned int n;
  vtkMRMLNode *node = NULL;

  for (n=0; n<numNodesInSceneView; n++) 
    {
    node  = dynamic_cast < vtkMRMLNode *>(this->Nodes->GetNodes()->GetItemAsObject(n));
    if (node) 
      {
      // for storage nodes replace full path with relative
      vtkMRMLStorageNode *snode = vtkMRMLStorageNode::SafeDownCast(node);
      if (snode)
        {
        vtkMRMLNode *node1 = this->Scene->GetNodeByID(snode->GetID());
        if (node1)
          {
          vtkMRMLStorageNode *snode1 = vtkMRMLStorageNode::SafeDownCast(node1);
          if (snode1)
            {
            snode->SetFileName(snode1->GetFileName());
            }
          }
        }
      } //if (node) 
    } //for (n=0; n<numNodesInSceneView; n++) 
}

//----------------------------------------------------------------------------
vtkMRMLStorageNode* vtkMRMLSceneViewNode::CreateDefaultStorageNode()
{
  return vtkMRMLSceneViewStorageNode::New();
}

//----------------------------------------------------------------------------
void vtkMRMLSceneViewNode::SetSceneViewDescription(const vtkStdString& newDescription)
{
  if (this->SceneViewDescription == newDescription)
    {
    return;
    }
  this->SceneViewDescription = newDescription;
  this->StorableModifiedTime.Modified();
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkMRMLSceneViewNode::SetScreenShot(vtkImageData* newScreenShot)
{
  this->StorableModifiedTime.Modified();
  //vtkSetObjectBodyMacro(ScreenShot, vtkImageData, newScreenShot);
  if (!newScreenShot)
    {
    if (this->ScreenShot)
      {
      this->ScreenShot->Delete();
      }
    this->ScreenShot = NULL;
    }
  else
    {
    if (!this->ScreenShot)
      {
      this->ScreenShot = vtkImageData::New();
      }
    this->ScreenShot->DeepCopy(newScreenShot);
    }
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkMRMLSceneViewNode::SetScreenShotType(int newScreenShotType)
{
  if (this->ScreenShotType == newScreenShotType)
    {
    return;
    }
  this->ScreenShotType = newScreenShotType;
  this->StorableModifiedTime.Modified();
  this->Modified();
}
//----------------------------------------------------------------------------
int vtkMRMLSceneViewNode::GetNodesByClass(const char *className, std::vector<vtkMRMLNode *> &nodes)
{
  return this->Nodes->GetNodesByClass(className, nodes);
}

//----------------------------------------------------------------------------
bool vtkMRMLSceneViewNode::IncludeNodeInSceneView(vtkMRMLNode *node)
{
  if (!node)
    {
    return false;
    }
  bool includeInView = true;
  // check for simple node types
  if (node->IsA("vtkMRMLSceneViewNode") ||
      node->IsA("vtkMRMLSceneViewStorageNode") ||
      node->IsA("vtkMRMLSnapshotClipNode"))
    {
    includeInView = false;
    }
  
  // check for scene view hierarchy nodes
  else if (node->IsA("vtkMRMLHierarchyNode"))
    {
    // check for tagged scene view hierarchy nodes
    if (node->GetAttribute("SceneViewHierarchy") != NULL)
      {
      includeInView = false;
      }
    // is it a top level singleton node?
    else if (node->GetID() && !strncmp(node->GetID(), "vtkMRMLHierarchyNodeSceneViewTopLevel", 37))
      {
      includeInView = false;
      }
    // backward compatibility: is it an old top level node?
    else if (node->GetName() && !strncmp(node->GetName(), "SceneViewToplevel", 17))
      {
      includeInView = false;
      }
    else
      {
      vtkSmartPointer<vtkMRMLHierarchyNode> hnode = vtkMRMLHierarchyNode::SafeDownCast(node);
      if (hnode)
        {
        // backward compatibility: non tagged hierarchy nodes
        // check for hierarchy nodes that point to scene view nodes
        if (hnode->GetAssociatedNode() && hnode->GetAssociatedNode()->IsA("vtkMRMLSceneViewNode"))
          {
          includeInView = false;
          }
        // getting the node might fail if it's an erroneously included hierarchy
        // node in a scene view node being restored
        else if (hnode->GetAssociatedNodeID() && !strncmp(hnode->GetAssociatedNodeID(), "vtkMRMLSceneViewNode", 20))
          {
          includeInView = false;
          }
        }
      }
    }
  
  vtkDebugMacro("IncludeNodeInSceneView: node " << node->GetID() << " includeInView = " << includeInView);
  
  return includeInView;
}

void vtkMRMLSceneViewNode::SetSceneViewRootDir( const char* name)
{
  this->Nodes->SetRootDirectory(name);
}