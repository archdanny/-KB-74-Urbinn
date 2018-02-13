   void System::SaveMapText() {
        std::string filename = "/Users/Danny/Documents/XcodePrograms/ORB_SLAM2/Examples/Monocular/mapText.txt";
        ///save keyframe and map points
        
        cout << endl << "Saving keypoints to " << filename << " ..." << endl;
        
        vector<KeyFrame *> vpKFs = mpMap->GetAllKeyFrames();
        sort(vpKFs.begin(), vpKFs.end(), KeyFrame::lId);
        

        cv::Mat Two = vpKFs[0]->GetPoseInverse();
        
        ofstream f;
        f.open(filename.c_str());
        f << fixed;
        

        list<ORB_SLAM2::KeyFrame *>::iterator lRit = mpTracker->mlpReferences.begin();
        list<double>::iterator lT = mpTracker->mlFrameTimes.begin();
        for (list<cv::Mat>::iterator lit = mpTracker->mlRelativeFramePoses.begin(), lend = mpTracker->mlRelativeFramePoses.end();
             lit != lend; lit++, lRit++, lT++) {
            ORB_SLAM2::KeyFrame *pKF = *lRit;
            
            if (pKF != NULL) {
                f << "KeyframeId : " << pKF->mnFrameId << " :" << endl;
                f << "Keyframe Pose : " << pKF->GetPoseInverse() << " :" << endl;
                f << "Keyframe Camera Center : " << pKF->GetCameraCenter() << ":" << endl;
                
                f << "Keyframe Timestamp : " << pKF->mTimeStamp << ":" << endl;
                
                vector<cv::KeyPoint > mapPoints = pKF->mvKeys;
                
                set<MapPoint *> points = pKF->GetMapPoints();
                
                for(auto mapPoint : points) {

                        // use f here
                f << "Key point X: " <<  mapPoint->pointImageX << "    Y: " << mapPoint->pointImageY<< endl;
                        
                        //auto check = pKF->GetPoseInverse().ptr<float>(0);
                f << "mappoint x: " <<  mapPoint->GetWorldPos().at<float>(0)  << " y:" << mapPoint->GetWorldPos().at<float>(2)  << "  z:" <<  mapPoint->GetWorldPos().at<float>(2) << endl;
                    
                    
                    cv::Mat Owi = pKF->GetCameraCenter();
                    cv::Mat normali = mapPoint->GetWorldPos() - Owi;
                    cv::Mat normal =  normali/cv::norm(normali);
                    
                    cv::Mat Pos =  mapPoint->GetWorldPos().clone();
                    cv::Mat PC = Pos - mapPoint->GetReferenceKeyFrame()->GetCameraCenter();
                    const float dist = cv::norm(PC);
                    const int level = mapPoint->GetReferenceKeyFrame()->mvKeysUn[mapPoint->GetObservations()[mapPoint->GetReferenceKeyFrame()]].octave;
                    const float levelScaleFactor =  mapPoint->GetReferenceKeyFrame()->mvScaleFactors[level];
                    
                    float distance = dist*levelScaleFactor;
                    
                    //double distance = hypot(hypot(pKF->GetPoseInverse().at<float>(0)-mapPoint->GetWorldPos().at<float>(0),pKF->GetPose().at<float>(1)-mapPoint->GetWorldPos().at<float>(1)),pKF->GetPose().at<float>(2)-mapPoint->GetWorldPos().at<float>(2));
                        f << "Distance : " <<  distance << endl;
                    }

            }
        }
        f.close();
        cout << endl << "key frames saved!" << endl;
    }
    
    
    void System::SaveMapPointsToCSV(const string &filename) {
        cout << endl << "Saving mapoints to " << filename << "..." << endl;
        
        vector<MapPoint *> mapPoints = mpMap->GetAllMapPoints();
        
        string localString = filename;
        localString.append(".csv");

        ofstream f;
        f.open(localString.c_str());
        f << fixed;
        
        f << "x, y, z" << endl;
        
        for(auto mapPoint : mapPoints) {
            f << mapPoint->GetWorldPos().at<float>(0) << "," << mapPoint->GetWorldPos().at<float>(1)  << "," << mapPoint->GetWorldPos().at<float>(2) << endl;
            
        }
        
        f.close();
        cout << endl << "Mappoints saved!" << endl;
    }
    
    
    bool System::SaveMapForXML(const string &filename) {
        
        //refrence keyframe and id
        std::map< KeyFrame* , ptree* > * storeKeyFrames = new std::map<KeyFrame* ,ptree* >();

        vector<MapPoint *> mapPoints = mpMap->GetAllMapPoints();
        
        ptree pt;
        
        string localStringTXT = filename;
        localStringTXT.append("XML.txt");
        
        //map -> attributes
        ptree mapTree;
        
        
        ptree keyframeTree;
        
        for(auto keyframe :  mpMap->GetAllKeyFrames()  ) {
            keyframeTree.push_back( ptree::value_type("KeyFrame", *keyframe->propertyTreeFromKeyframe(storeKeyFrames) ));
        }
        mapTree.add_child("KeyFrames", keyframeTree);
        
        
        //map -> mappoints
        ptree mapPointsTree;
        
        for(auto mapPoint : mapPoints) {
            ptree mapPointLocal;
            mapPointLocal.put("x", mapPoint->GetWorldPos().at<float>(0));
            mapPointLocal.put("y", mapPoint->GetWorldPos().at<float>(1));
            mapPointLocal.put("z", mapPoint->GetWorldPos().at<float>(2));
            
            mapPointsTree.push_back(ptree::value_type("mappoint", mapPointLocal));
        }
        
        //add mappoints to map tree
        mapTree.add_child("mappoints", mapPointsTree);
        
        
        //output file
        write_xml(localStringTXT, mapTree, std::locale(), boost::property_tree::xml_writer_make_settings<boost::property_tree::ptree::key_type>(' ', 1u));
        
        return true;
    }
    
    bool fileExists(const std::string& filename) {
        struct stat buf;
        if (stat(filename.c_str(), &buf) != -1)
        {
            return true;
        }
        return false;
    }

    bool System::LoadMapFromXML(const string &filename) {
        // populate tree structure pt
        string localStringTXT = filename;
        localStringTXT.append("XML.txt");

        if (!fileExists(localStringTXT)) {
            return false;
        }

        using boost::property_tree::ptree;
        ptree pt;
        read_xml(localStringTXT, pt);
        
        std::vector<KeyFrame*> keyframes = KeyFrame::keyFrameFromPropertyTree(pt);
        
        return true;
    }


    bool System::LoadMap(const string &filename) {
        //new
        
        {
            string localString = filename;
            localString.append(".bin");
            std::ifstream is(localString, std::ios_base::binary);
            if (!is) {
                cerr << "Cannot Open Mapfile: " << mapfile << ", Create a new one" << std::endl;
                return false;
            }
            boost::archive::binary_iarchive ia(is, boost::archive::no_header);

            ia >> mpMap;


        }

        cout << endl << filename <<" : Map Loaded!" << endl;
        return true;

    }