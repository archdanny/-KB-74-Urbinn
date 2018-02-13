        
           
{   
    if (!mapfile.empty() && LoadMap(mapfile))
    {
        bReuseMap = true;
    }
    else
    {
        mpMap = new Map();
    }
    
    
    if (bReuseMap)
    {
        
        vector<ORB_SLAM2::KeyFrame*> vpKFs = mpMap->GetAllKeyFrames();
        for (vector<ORB_SLAM2::KeyFrame*>::iterator it = vpKFs.begin(); it != vpKFs.end(); ++it) {
            (*it)->SetKeyFrameDatabase(mpKeyFrameDatabase);
            (*it)->SetORBvocabulary(mpVocabulary);
            (*it)->SetMap(mpMap);
            (*it)->ComputeBoW();
            mpKeyFrameDatabase->add(*it);
            (*it)->SetMapPoints(mpMap->GetAllMapPoints());
            (*it)->SetSpanningTree(vpKFs);
            (*it)->SetGridParams(vpKFs);
            
            // Reconstruct map points Observation
            
        }
        
        vector<ORB_SLAM2::MapPoint*> vpMPs = mpMap->GetAllMapPoints();
        for (vector<ORB_SLAM2::MapPoint*>::iterator mit = vpMPs.begin(); mit != vpMPs.end(); ++mit) {
            (*mit)->SetMap(mpMap);
            (*mit)->SetObservations(vpKFs);
            
        }
        
        for (vector<ORB_SLAM2::KeyFrame*>::iterator it = vpKFs.begin(); it != vpKFs.end(); ++it) {
            (*it)->UpdateConnections();
        }
        
        
    }

}