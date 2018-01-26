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