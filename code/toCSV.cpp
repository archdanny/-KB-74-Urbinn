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
    