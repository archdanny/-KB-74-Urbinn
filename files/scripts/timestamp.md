```javascript
        std::string filename = "/Users/Danny/Documents/timestamps";
        cout << endl << "Saving mapoints to " << filename << "..." << endl;
    
        string localString = filename;
        localString.append(".txt");
    
        ofstream f;
        f.open(localString.c_str());
        f << fixed;
    
    
        for( int i = 0; i< 14438; i++  ) {  //  for( int i = 0; i< 33066 / 2; i++  ) {
            f <<  boost::posix_time::microsec_clock::universal_time() << endl;
    
        }
    
        f.close();
        cout << endl << "timestamps saved!" << endl;
```