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
