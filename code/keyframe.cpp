/**
* This file is part of ORB-SLAM2.
*
* Copyright (C) 2014-2016 Raúl Mur-Artal <raulmur at unizar dot es> (University of Zaragoza)
* For more information see <https://github.com/raulmur/ORB_SLAM2>
*
* ORB-SLAM2 is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* ORB-SLAM2 is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with ORB-SLAM2. If not, see <http://www.gnu.org/licenses/>.
*/

#include "KeyFrame.h"
#include "Converter.h"
#include "ORBmatcher.h"
#include<mutex>
#include <thread>



namespace ORB_SLAM2 {

    long unsigned int KeyFrame::nNextId = 0;

    KeyFrame::KeyFrame(Frame &F, Map *pMap, KeyFrameDatabase *pKFDB) :
            mnFrameId(F.mnId), mTimeStamp(F.mTimeStamp), mnGridCols(FRAME_GRID_COLS), mnGridRows(FRAME_GRID_ROWS),
            mfGridElementWidthInv(F.mfGridElementWidthInv), mfGridElementHeightInv(F.mfGridElementHeightInv),
            mnTrackReferenceForFrame(0), mnFuseTargetForKF(0), mnBALocalForKF(0), mnBAFixedForKF(0),
            mnLoopQuery(0), mnLoopWords(0), mnRelocQuery(0), mnRelocWords(0), mnBAGlobalForKF(0),
            fx(F.fx), fy(F.fy), cx(F.cx), cy(F.cy), invfx(F.invfx), invfy(F.invfy),
            mbf(F.mbf), mb(F.mb), mThDepth(F.mThDepth), N(F.N), mvKeys(F.mvKeys), mvKeysUn(F.mvKeysUn),
            mvuRight(F.mvuRight), mvDepth(F.mvDepth), mDescriptors(F.mDescriptors.clone()),
            mBowVec(F.mBowVec), mFeatVec(F.mFeatVec), mnScaleLevels(F.mnScaleLevels), mfScaleFactor(F.mfScaleFactor),
            mfLogScaleFactor(F.mfLogScaleFactor), mvScaleFactors(F.mvScaleFactors), mvLevelSigma2(F.mvLevelSigma2),
            mvInvLevelSigma2(F.mvInvLevelSigma2), mnMinX(F.mnMinX), mnMinY(F.mnMinY), mnMaxX(F.mnMaxX),
            mnMaxY(F.mnMaxY), mK(F.mK), mvpMapPoints(F.mvpMapPoints), mpKeyFrameDB(pKFDB),
            mpORBvocabulary(F.mpORBvocabulary), mbFirstConnection(true), mpParent(NULL), mbNotErase(false),
            mbToBeErased(false), mbBad(false), mHalfBaseline(F.mb / 2), mpMap(pMap) {
        mnId = nNextId++;

        mGrid.resize(mnGridCols);
        for (int i = 0; i < mnGridCols; i++) {
            mGrid[i].resize(mnGridRows);
            for (int j = 0; j < mnGridRows; j++)
                mGrid[i][j] = F.mGrid[i][j];
        }

        SetPose(F.mTcw);
    }

    void KeyFrame::ComputeBoW() {
        if (mBowVec.empty() || mFeatVec.empty()) {
            vector<cv::Mat> vCurrentDesc = Converter::toDescriptorVector(mDescriptors);
            // Feature vector associate features with nodes in the 4th level (from leaves up)
            // We assume the vocabulary tree has 6 levels, change the 4 otherwise
            mpORBvocabulary->transform(vCurrentDesc, mBowVec, mFeatVec, 4);
        }
    }

    void KeyFrame::SetPose(const cv::Mat &Tcw_) {
        unique_lock<mutex> lock(mMutexPose);
        Tcw_.copyTo(Tcw);
        cv::Mat Rcw = Tcw.rowRange(0, 3).colRange(0, 3);
        cv::Mat tcw = Tcw.rowRange(0, 3).col(3);
        cv::Mat Rwc = Rcw.t();
        Ow = -Rwc * tcw;

        Twc = cv::Mat::eye(4, 4, Tcw.type());
        Rwc.copyTo(Twc.rowRange(0, 3).colRange(0, 3));
        Ow.copyTo(Twc.rowRange(0, 3).col(3));
        cv::Mat center = (cv::Mat_<float>(4, 1) << mHalfBaseline, 0, 0, 1);
        Cw = Twc * center;
        
    }

    cv::Mat KeyFrame::GetPose() {
        unique_lock<mutex> lock(mMutexPose);
        cv::Mat clone =  Tcw.clone();
        lock.unlock();
        return clone;
    }
    
    cv::Mat KeyFrame::GetPoseInverseForMultiplication() {
        return Twc;
    }

    cv::Mat KeyFrame::GetPoseInverse() {
//        std::thread::id this_id = std::this_thread::get_id();
//        std::cout << "thread " << this_id << endl;
        
        unique_lock<mutex> lock(mMutexPose);
        return Twc.clone();
        
        
//        unique_lock<mutex> lock(mMutexPose);
//        return Twc.clone();
    }

    cv::Mat KeyFrame::GetCameraCenter() {
        unique_lock<mutex> lock(mMutexPose);
        return Ow.clone();
    }

    cv::Mat KeyFrame::GetStereoCenter() {
        unique_lock<mutex> lock(mMutexPose);
        return Cw.clone();
    }


    cv::Mat KeyFrame::GetRotation() {
        unique_lock<mutex> lock(mMutexPose);
        return Tcw.rowRange(0, 3).colRange(0, 3).clone();
    }

    cv::Mat KeyFrame::GetTranslation() {
        unique_lock<mutex> lock(mMutexPose);
        return Tcw.rowRange(0, 3).col(3).clone();
    }

    void KeyFrame::AddConnection(KeyFrame *pKF, const int &weight) {
        {
            unique_lock<mutex> lock(mMutexConnections);
            if (!mConnectedKeyFrameWeights.count(pKF))
                mConnectedKeyFrameWeights[pKF] = weight;
            else if (mConnectedKeyFrameWeights[pKF] != weight)
                mConnectedKeyFrameWeights[pKF] = weight;
            else
                return;
        }

        UpdateBestCovisibles();
    }

    void KeyFrame::UpdateBestCovisibles() {
        unique_lock<mutex> lock(mMutexConnections);
        vector<pair<int, KeyFrame *> > vPairs;
        vPairs.reserve(mConnectedKeyFrameWeights.size());
        for (map<KeyFrame *, int>::iterator mit = mConnectedKeyFrameWeights.begin(), mend = mConnectedKeyFrameWeights.end();
             mit != mend; mit++)
            vPairs.push_back(make_pair(mit->second, mit->first));

        sort(vPairs.begin(), vPairs.end());
        list<KeyFrame *> lKFs;
        list<int> lWs;
        for (size_t i = 0, iend = vPairs.size(); i < iend; i++) {
            lKFs.push_front(vPairs[i].second);
            lWs.push_front(vPairs[i].first);
        }

        mvpOrderedConnectedKeyFrames = vector<KeyFrame *>(lKFs.begin(), lKFs.end());
        mvOrderedWeights = vector<int>(lWs.begin(), lWs.end());
    }

    set<KeyFrame *> KeyFrame::GetConnectedKeyFrames() {
        unique_lock<mutex> lock(mMutexConnections);
        set<KeyFrame *> s;
        for (map<KeyFrame *, int>::iterator mit = mConnectedKeyFrameWeights.begin();
             mit != mConnectedKeyFrameWeights.end(); mit++)
            s.insert(mit->first);
        return s;
    }

    vector<KeyFrame *> KeyFrame::GetVectorCovisibleKeyFrames() {
        unique_lock<mutex> lock(mMutexConnections);
        return mvpOrderedConnectedKeyFrames;
    }

    vector<KeyFrame *> KeyFrame::GetBestCovisibilityKeyFrames(const int &N) {
        unique_lock<mutex> lock(mMutexConnections);
        if ((int) mvpOrderedConnectedKeyFrames.size() < N)
            return mvpOrderedConnectedKeyFrames;
        else
            return vector<KeyFrame *>(mvpOrderedConnectedKeyFrames.begin(), mvpOrderedConnectedKeyFrames.begin() + N);

    }

    vector<KeyFrame *> KeyFrame::GetCovisiblesByWeight(const int &w) {
        unique_lock<mutex> lock(mMutexConnections);

        if (mvpOrderedConnectedKeyFrames.empty())
            return vector<KeyFrame *>();

        vector<int>::iterator it = upper_bound(mvOrderedWeights.begin(), mvOrderedWeights.end(), w,
                                               KeyFrame::weightComp);
        if (it == mvOrderedWeights.end())
            return vector<KeyFrame *>();
        else {
            int n = it - mvOrderedWeights.begin();
            return vector<KeyFrame *>(mvpOrderedConnectedKeyFrames.begin(), mvpOrderedConnectedKeyFrames.begin() + n);
        }
    }

    int KeyFrame::GetWeight(KeyFrame *pKF) {
        unique_lock<mutex> lock(mMutexConnections);
        if (mConnectedKeyFrameWeights.count(pKF))
            return mConnectedKeyFrameWeights[pKF];
        else
            return 0;
    }

    void KeyFrame::AddMapPoint(MapPoint *pMP, const size_t &idx) {
        unique_lock<mutex> lock(mMutexFeatures);
        mvpMapPoints[idx] = pMP;
    }

    void KeyFrame::EraseMapPointMatch(const size_t &idx) {
        unique_lock<mutex> lock(mMutexFeatures);
        mvpMapPoints[idx] = static_cast<MapPoint *>(NULL);
    }

    void KeyFrame::EraseMapPointMatch(MapPoint *pMP) {
        int idx = pMP->GetIndexInKeyFrame(this);
        if (idx >= 0)
            mvpMapPoints[idx] = static_cast<MapPoint *>(NULL);
    }


    void KeyFrame::ReplaceMapPointMatch(const size_t &idx, MapPoint *pMP) {
        mvpMapPoints[idx] = pMP;
    }

    set<MapPoint *> KeyFrame::GetMapPoints() {
        unique_lock<mutex> lock(mMutexFeatures);
        set<MapPoint *> s;
        for (size_t i = 0, iend = mvpMapPoints.size(); i < iend; i++) {
            if (!mvpMapPoints[i])
                continue;
            MapPoint *pMP = mvpMapPoints[i];
            if (!pMP->isBad())
                s.insert(pMP);
        }
        return s;
    }

    int KeyFrame::TrackedMapPoints(const int &minObs) {
        unique_lock<mutex> lock(mMutexFeatures);

        int nPoints = 0;
        const bool bCheckObs = minObs > 0;
        for (int i = 0; i < N; i++) {
            MapPoint *pMP = mvpMapPoints[i];
            if (pMP) {
                if (!pMP->isBad()) {
                    if (bCheckObs) {
                        if (mvpMapPoints[i]->Observations() >= minObs)
                            nPoints++;
                    } else
                        nPoints++;
                }
            }
        }

        return nPoints;
    }

    vector<MapPoint *> KeyFrame::GetMapPointMatches() {
        unique_lock<mutex> lock(mMutexFeatures);
        return mvpMapPoints;
    }

    MapPoint *KeyFrame::GetMapPoint(const size_t &idx) {
        unique_lock<mutex> lock(mMutexFeatures);
        return mvpMapPoints[idx];
    }

    void KeyFrame::UpdateConnections() {
        map<KeyFrame *, int> KFcounter;

        vector<MapPoint *> vpMP;

        {
            unique_lock<mutex> lockMPs(mMutexFeatures);
            vpMP = mvpMapPoints;
        }

        //For all map points in keyframe check in which other keyframes are they seen
        //Increase counter for those keyframes
        for (vector<MapPoint *>::iterator vit = vpMP.begin(), vend = vpMP.end(); vit != vend; vit++) {
            MapPoint *pMP = *vit;

            if (!pMP)
                continue;

            if (pMP->isBad())
                continue;

            map<KeyFrame *, size_t> observations = pMP->GetObservations();

            for (map<KeyFrame *, size_t>::iterator mit = observations.begin(), mend = observations.end();
                 mit != mend; mit++) {
                if (mit->first->mnId == mnId)
                    continue;
                KFcounter[mit->first]++;
            }
        }

        // This should not happen
        if (KFcounter.empty())
            return;

        //If the counter is greater than threshold add connection
        //In case no keyframe counter is over threshold add the one with maximum counter
        int nmax = 0;
        KeyFrame *pKFmax = NULL;
        int th = 15;

        vector<pair<int, KeyFrame *> > vPairs;
        vPairs.reserve(KFcounter.size());
        for (map<KeyFrame *, int>::iterator mit = KFcounter.begin(), mend = KFcounter.end(); mit != mend; mit++) {
            if (mit->second > nmax) {
                nmax = mit->second;
                pKFmax = mit->first;
            }
            if (mit->second >= th) {
                vPairs.push_back(make_pair(mit->second, mit->first));
                (mit->first)->AddConnection(this, mit->second);
            }
        }

        if (vPairs.empty()) {
            vPairs.push_back(make_pair(nmax, pKFmax));
            pKFmax->AddConnection(this, nmax);
        }

        sort(vPairs.begin(), vPairs.end());
        list<KeyFrame *> lKFs;
        list<int> lWs;
        for (size_t i = 0; i < vPairs.size(); i++) {
            lKFs.push_front(vPairs[i].second);
            lWs.push_front(vPairs[i].first);
        }

        {
            unique_lock<mutex> lockCon(mMutexConnections);

            // mspConnectedKeyFrames = spConnectedKeyFrames;
            mConnectedKeyFrameWeights = KFcounter;
            mvpOrderedConnectedKeyFrames = vector<KeyFrame *>(lKFs.begin(), lKFs.end());
            mvOrderedWeights = vector<int>(lWs.begin(), lWs.end());

            if (mbFirstConnection && mnId != 0) {
                mpParent = mvpOrderedConnectedKeyFrames.front();
                mpParent->AddChild(this);
                mbFirstConnection = false;
            }

        }
    }

    void KeyFrame::AddChild(KeyFrame *pKF) {
        unique_lock<mutex> lockCon(mMutexConnections);
        mspChildrens.insert(pKF);
    }

    void KeyFrame::EraseChild(KeyFrame *pKF) {
        unique_lock<mutex> lockCon(mMutexConnections);
        mspChildrens.erase(pKF);
    }

    void KeyFrame::ChangeParent(KeyFrame *pKF) {
        unique_lock<mutex> lockCon(mMutexConnections);
        mpParent = pKF;
        pKF->AddChild(this);
    }

    set<KeyFrame *> KeyFrame::GetChilds() {
        unique_lock<mutex> lockCon(mMutexConnections);
        return mspChildrens;
    }

    KeyFrame *KeyFrame::GetParent() {
        unique_lock<mutex> lockCon(mMutexConnections);
        return mpParent;
    }

    bool KeyFrame::hasChild(KeyFrame *pKF) {
        unique_lock<mutex> lockCon(mMutexConnections);
        return mspChildrens.count(pKF);
    }

    void KeyFrame::AddLoopEdge(KeyFrame *pKF) {
        unique_lock<mutex> lockCon(mMutexConnections);
        mbNotErase = true;
        mspLoopEdges.insert(pKF);
    }

    set<KeyFrame *> KeyFrame::GetLoopEdges() {
        unique_lock<mutex> lockCon(mMutexConnections);
        return mspLoopEdges;
    }

    void KeyFrame::SetNotErase() {
        unique_lock<mutex> lock(mMutexConnections);
        mbNotErase = true;
    }

    void KeyFrame::SetErase() {
        {
            unique_lock<mutex> lock(mMutexConnections);
            if (mspLoopEdges.empty()) {
                mbNotErase = false;
            }
        }

        if (mbToBeErased) {
            SetBadFlag();
        }
    }

    void KeyFrame::SetBadFlag() {
        {
            unique_lock<mutex> lock(mMutexConnections);
            if (mnId == 0)
                return;
            else if (mbNotErase) {
                mbToBeErased = true;
                return;
            }
        }

        for (map<KeyFrame *, int>::iterator mit = mConnectedKeyFrameWeights.begin(), mend = mConnectedKeyFrameWeights.end();
             mit != mend; mit++)
            mit->first->EraseConnection(this);

        for (size_t i = 0; i < mvpMapPoints.size(); i++)
            if (mvpMapPoints[i])
                mvpMapPoints[i]->EraseObservation(this);
        {
            unique_lock<mutex> lock(mMutexConnections);
            unique_lock<mutex> lock1(mMutexFeatures);

            mConnectedKeyFrameWeights.clear();
            mvpOrderedConnectedKeyFrames.clear();

            // Update Spanning Tree
            set<KeyFrame *> sParentCandidates;
            sParentCandidates.insert(mpParent);

            // Assign at each iteration one children with a parent (the pair with highest covisibility weight)
            // Include that children as new parent candidate for the rest
            while (!mspChildrens.empty()) {
                bool bContinue = false;

                int max = -1;
                KeyFrame *pC;
                KeyFrame *pP;

                for (set<KeyFrame *>::iterator sit = mspChildrens.begin(), send = mspChildrens.end();
                     sit != send; sit++) {
                    KeyFrame *pKF = *sit;
                    if (pKF->isBad())
                        continue;

                    // Check if a parent candidate is connected to the keyframe
                    vector<KeyFrame *> vpConnected = pKF->GetVectorCovisibleKeyFrames();
                    for (size_t i = 0, iend = vpConnected.size(); i < iend; i++) {
                        for (set<KeyFrame *>::iterator spcit = sParentCandidates.begin(), spcend = sParentCandidates.end();
                             spcit != spcend; spcit++) {
                            if (vpConnected[i]->mnId == (*spcit)->mnId) {
                                int w = pKF->GetWeight(vpConnected[i]);
                                if (w > max) {
                                    pC = pKF;
                                    pP = vpConnected[i];
                                    max = w;
                                    bContinue = true;
                                }
                            }
                        }
                    }
                }

                if (bContinue) {
                    pC->ChangeParent(pP);
                    sParentCandidates.insert(pC);
                    mspChildrens.erase(pC);
                } else
                    break;
            }

            // If a children has no covisibility links with any parent candidate, assign to the original parent of this KF
            if (!mspChildrens.empty())
                for (set<KeyFrame *>::iterator sit = mspChildrens.begin(); sit != mspChildrens.end(); sit++) {
                    (*sit)->ChangeParent(mpParent);
                }

            mpParent->EraseChild(this);
            mTcp = Tcw * mpParent->GetPoseInverse();
            mbBad = true;
        }


        mpMap->EraseKeyFrame(this);
        mpKeyFrameDB->erase(this);
    }

    bool KeyFrame::isBad() {
        unique_lock<mutex> lock(mMutexConnections);
        return mbBad;
    }

    void KeyFrame::EraseConnection(KeyFrame *pKF) {
        bool bUpdate = false;
        {
            unique_lock<mutex> lock(mMutexConnections);
            if (mConnectedKeyFrameWeights.count(pKF)) {
                mConnectedKeyFrameWeights.erase(pKF);
                bUpdate = true;
            }
        }

        if (bUpdate)
            UpdateBestCovisibles();
    }

    vector<size_t> KeyFrame::GetFeaturesInArea(const float &x, const float &y, const float &r) const {
        vector<size_t> vIndices;
        vIndices.reserve(N);

        const int nMinCellX = max(0, (int) floor((x - mnMinX - r) * mfGridElementWidthInv));
        if (nMinCellX >= mnGridCols)
            return vIndices;

        const int nMaxCellX = min((int) mnGridCols - 1, (int) ceil((x - mnMinX + r) * mfGridElementWidthInv));
        if (nMaxCellX < 0)
            return vIndices;

        const int nMinCellY = max(0, (int) floor((y - mnMinY - r) * mfGridElementHeightInv));
        if (nMinCellY >= mnGridRows)
            return vIndices;

        const int nMaxCellY = min((int) mnGridRows - 1, (int) ceil((y - mnMinY + r) * mfGridElementHeightInv));
        if (nMaxCellY < 0)
            return vIndices;

        for (int ix = nMinCellX; ix <= nMaxCellX; ix++) {
            for (int iy = nMinCellY; iy <= nMaxCellY; iy++) {
                const vector<size_t> vCell = mGrid[ix][iy];
                for (size_t j = 0, jend = vCell.size(); j < jend; j++) {
                    const cv::KeyPoint &kpUn = mvKeysUn[vCell[j]];
                    const float distx = kpUn.pt.x - x;
                    const float disty = kpUn.pt.y - y;

                    if (fabs(distx) < r && fabs(disty) < r)
                        vIndices.push_back(vCell[j]);
                }
            }
        }

        return vIndices;
    }

    bool KeyFrame::IsInImage(const float &x, const float &y) const {
        return (x >= mnMinX && x < mnMaxX && y >= mnMinY && y < mnMaxY);
    }

    cv::Mat KeyFrame::UnprojectStereo(int i) {
        const float z = mvDepth[i];
        if (z > 0) {
            const float u = mvKeys[i].pt.x;
            const float v = mvKeys[i].pt.y;
            const float x = (u - cx) * z * invfx;
            const float y = (v - cy) * z * invfy;
            cv::Mat x3Dc = (cv::Mat_<float>(3, 1) << x, y, z);

            unique_lock<mutex> lock(mMutexPose);
            return Twc.rowRange(0, 3).colRange(0, 3) * x3Dc + Twc.rowRange(0, 3).col(3);
        } else
            return cv::Mat();
    }

    float KeyFrame::ComputeSceneMedianDepth(const int q) {
        vector<MapPoint *> vpMapPoints;
        cv::Mat Tcw_;
        {
            unique_lock<mutex> lock(mMutexFeatures);
            unique_lock<mutex> lock2(mMutexPose);
            vpMapPoints = mvpMapPoints;
            Tcw_ = Tcw.clone();
        }

        vector<float> vDepths;
        vDepths.reserve(N);
        cv::Mat Rcw2 = Tcw_.row(2).colRange(0, 3);
        Rcw2 = Rcw2.t();
        float zcw = Tcw_.at<float>(2, 3);
        for (int i = 0; i < N; i++) {
            if (mvpMapPoints[i]) {
                MapPoint *pMP = mvpMapPoints[i];
                cv::Mat x3Dw = pMP->GetWorldPos();
                float z = Rcw2.dot(x3Dw) + zcw;
                vDepths.push_back(z);
            }
        }

        sort(vDepths.begin(), vDepths.end());

        return vDepths[(vDepths.size() - 1) / q];
    }

// Default serializing Constructor
//    KeyFrame::KeyFrame() :
//            mnFrameId(0), mTimeStamp(0.0), mnGridCols(FRAME_GRID_COLS), mnGridRows(FRAME_GRID_ROWS),
//            mfGridElementWidthInv(0.0), mfGridElementHeightInv(0.0),
//            mnTrackReferenceForFrame(0), mnFuseTargetForKF(0), mnBALocalForKF(0), mnBAFixedForKF(0),
//            mnLoopQuery(0), mnLoopWords(0), mnRelocQuery(0), mnRelocWords(0), mnBAGlobalForKF(0),
//            fx(0.0), fy(0.0), cx(0.0), cy(0.0), invfx(0.0), invfy(0.0),
//            mbf(0.0), mb(0.0), mThDepth(0.0), N(0), mnScaleLevels(0), mfScaleFactor(0),
//            mfLogScaleFactor(0.0),
//            mnMinX(0), mnMinY(0), mnMaxX(0),
//            mnMaxY(0) {}
//
//    template<class Archive>
//    void KeyFrame::serialize(Archive &ar, const unsigned int version) {
//        // no mutex needed vars
//        ar & nNextId;
//        ar & mnId;
//        ar & const_cast<long unsigned int &>(mnFrameId);
//        ar & const_cast<double &>(mTimeStamp);
//        // Grid related vars
//        ar & const_cast<int &>(mnGridCols);
//        ar & const_cast<int &>(mnGridRows);
//        ar & const_cast<float &>(mfGridElementWidthInv);
//        ar & const_cast<float &>(mfGridElementHeightInv);
//        // Tracking related vars
//        ar & mnTrackReferenceForFrame & mnFuseTargetForKF;
//        // LocalMaping related vars
//        ar & mnBALocalForKF & mnBAFixedForKF;
//        // KeyFrameDB related vars
//        ar & mnLoopQuery & mnLoopWords & mLoopScore & mnRelocQuery & mnRelocWords & mRelocScore;
//        // LoopClosing related vars
//        ar & mTcwGBA ; // & mTcwBefGBA & mnBAGlobalForKF;
//        ar & mTcwBefGBA ;
//        ar & mnBAGlobalForKF ;
//        // calibration parameters
//        ar & const_cast<float &>(fx) & const_cast<float &>(fy) & const_cast<float &>(cx) & const_cast<float &>(cy);
//        ar & const_cast<float &>(invfx) & const_cast<float &>(invfy) & const_cast<float &>(mbf);
//        ar & const_cast<float &>(mb) & const_cast<float &>(mThDepth);
//        // Number of KeyPoints;
//        ar & const_cast<int &>(N);
//        // KeyPoints, stereo coordinate and descriptors
//        ar & const_cast<std::vector<cv::KeyPoint> &>(mvKeys);
//        ar & const_cast<std::vector<cv::KeyPoint> &>(mvKeysUn);
//        ar & const_cast<std::vector<float> &>(mvuRight);
//        ar & const_cast<std::vector<float> &>(mvDepth);
//        ar & const_cast<cv::Mat &>(mDescriptors);
//        // Bow
//        ar & mBowVec & mFeatVec;
//        // Pose relative to parent
//        ar & mTcp;
//        // Scale related
//        ar & const_cast<int &>(mnScaleLevels) & const_cast<float &>(mfScaleFactor) &
//        const_cast<float &>(mfLogScaleFactor);
//        ar & const_cast<std::vector<float> &>(mvScaleFactors) & const_cast<std::vector<float> &>(mvLevelSigma2) &
//        const_cast<std::vector<float> &>(mvInvLevelSigma2);
//        // Image bounds and calibration
//        ar & const_cast<int &>(mnMinX) & const_cast<int &>(mnMinY) & const_cast<int &>(mnMaxX) &
//        const_cast<int &>(mnMaxY);
//        ar & const_cast<cv::Mat &>(mK);
//
//        // mutex needed vars, but don't lock mutex in the save/load procedure
//        {
//            unique_lock<mutex> lock_pose(mMutexPose);
//            ar & Tcw & Twc & Ow & Cw;
//        }
//        {
//            unique_lock<mutex> lock_feature(mMutexFeatures);
//            for (auto point : mvpMapPoints) {
//                ar & point; // hope boost deal with the pointer graph well
//            }
//
//        }
//        // BoW
//        ar & mpKeyFrameDB;
//        // mpORBvocabulary restore elsewhere(see SetORBvocab)
//        {
//            // Grid related
//            unique_lock<mutex> lock_connection(mMutexConnections);
//            ar & mGrid & mConnectedKeyFrameWeights & mvpOrderedConnectedKeyFrames & mvOrderedWeights;
//            // Spanning Tree and Loop Edges
//            ar & mbFirstConnection & mpParent & mspChildrens & mspLoopEdges;
//            // Bad flags
//            ar & mbNotErase & mbToBeErased & mbBad & mHalfBaseline;
//        }
//        // Map Points
//        ar & mpMap;
//        // don't save mutex
//    }
//
//    template void KeyFrame::serialize(boost::archive::binary_iarchive&, const unsigned int);
//    template void KeyFrame::serialize(boost::archive::binary_oarchive&, const unsigned int);
    
    
    //new
    
    // Default serializing Constructor
    KeyFrame::KeyFrame():
    mnFrameId(0),  mTimeStamp(0.0), mnGridCols(FRAME_GRID_COLS), mnGridRows(FRAME_GRID_ROWS),
    mfGridElementWidthInv(0.0), mfGridElementHeightInv(0.0),
    mnTrackReferenceForFrame(0), mnFuseTargetForKF(0), mnBALocalForKF(0), mnBAFixedForKF(0),
    mnLoopQuery(0), mnLoopWords(0), mnRelocQuery(0), mnRelocWords(0), mnBAGlobalForKF(0),
    fx(0.0), fy(0.0), cx(0.0), cy(0.0), invfx(0.0), invfy(0.0),
    mbf(0.0), mb(0.0), mThDepth(0.0), N(0), mnScaleLevels(0), mfScaleFactor(0),
    mfLogScaleFactor(0.0),
    mnMinX(0), mnMinY(0), mnMaxX(0),
    mnMaxY(0)
    {
#if 0
        mnId=nNextId++;
        mGrid.resize(mnGridCols);
        for(int i=0; i<mnGridCols;i++)
        {
            mGrid[i].resize(mnGridRows);
            for(int j=0; j<mnGridRows; j++)
                mGrid[i][j] = F.mGrid[i][j];
        }
        SetPose(F.mTcw);
#endif
    }
    
    template<class Archive>
    void KeyFrame::save(Archive & ar, const unsigned int version) const
    {
        int nItems;bool is_id;bool has_parent = false;
        long unsigned int t_nId;
        //int ordered_weight;
        ar & nNextId;
        int ConKfWeight;
        
        //if (mbToBeErased)
        //return;
        //if (mbBad)
        //return;
        ar & const_cast<long unsigned int &> (mnId);
        ar & const_cast<long unsigned int &> (mnFrameId);
        ar & const_cast<double &> (mTimeStamp);
        ar & const_cast<int &> (mnGridCols);
        ar & const_cast<int &> (mnGridRows);
        ar & const_cast<float &>  (mfGridElementWidthInv);
        ar & const_cast<float &>  (mfGridElementHeightInv);
        ar & const_cast<long unsigned int &> (mnTrackReferenceForFrame);
        ar & const_cast<long unsigned int &> (mnFuseTargetForKF);
        ar & const_cast<long unsigned int &> (mnBALocalForKF);
        ar & const_cast<long unsigned int &> (mnBAFixedForKF);
        ar & const_cast<long unsigned int &> (mnLoopQuery);
        ar & const_cast<int &> (mnLoopWords);
        ar & const_cast<float &> (mLoopScore);
        ar & const_cast<long unsigned int &> (mnRelocQuery);
        ar & const_cast<int &> (mnRelocWords);
        ar & const_cast<float &> (mRelocScore);
        ar & const_cast<cv::Mat &> (mTcwGBA);
        ar & const_cast<cv::Mat &> (mTcwBefGBA);
        ar & const_cast<long unsigned int &> (mnBAGlobalForKF);
        ar & const_cast<float &> (fx);
        ar & const_cast<float &> (fy);
        ar & const_cast<float &> (cx);
        ar & const_cast<float &> (cy);
        ar & const_cast<float &> (invfx);
        ar & const_cast<float &> (invfy);
        ar & const_cast<float &> (mbf);
        ar & const_cast<float &> (mb);
        ar & const_cast<float &> (mThDepth);
        ar & const_cast<int &> (N);
        ar & const_cast<std::vector<cv::KeyPoint> &> (mvKeys);
        ar & const_cast<std::vector<cv::KeyPoint> &> (mvKeysUn);
        ar & const_cast<std::vector<float> &> (mvuRight);
        ar & const_cast<std::vector<float> &> (mvDepth);
        ar & const_cast<cv::Mat &> (mDescriptors);
        ar & const_cast<cv::Mat &> (mTcp);
        ar & const_cast<int &> (mnScaleLevels);
        ar & const_cast<float &> (mfScaleFactor);
        ar & const_cast<float &> (mfLogScaleFactor);
        ar & const_cast<std::vector<float> &> (mvScaleFactors);
        ar & const_cast<std::vector<float> &> (mvLevelSigma2);
        ar & const_cast<std::vector<float> &> (mvInvLevelSigma2);
        
        ar & const_cast<int &> (mnMinX);
        ar & const_cast<int &> (mnMinY);
        ar & const_cast<int &> (mnMaxX);
        ar & const_cast<int &> (mnMaxY);
        ar & const_cast<cv::Mat &> (mK);
        ar & const_cast<cv::Mat &> (Tcw);
        ar & const_cast<cv::Mat &> (Twc);
        ar & const_cast<cv::Mat &> (Ow);
        ar & const_cast<cv::Mat &> (Cw);
        // Save each map point id
        nItems = mvpMapPoints.size();
        ar & nItems;
        
        //cout << "{INFO}mvpMapPoints nItems -" << nItems << endl;
        for (std::vector<MapPoint*>::const_iterator it = mvpMapPoints.begin(); it != mvpMapPoints.end(); ++it) {
            if (*it == NULL)
            {
                is_id = false;
                ar & is_id;
                continue;
            }
            else
            {
                is_id = true;
                ar & is_id;
                t_nId =  (**it).mnId;
                //cout << "[" << t_nId <<"]";
                
                ar & t_nId;
                
            }
            
            
        }
        
        // Grid
        ar & const_cast<std::vector< std::vector <std::vector<size_t> > > &> (mGrid);
        nItems = mConnectedKeyFrameWeights.size();
        ar & nItems;
        
        for (std::map<KeyFrame*,int>::const_iterator it = mConnectedKeyFrameWeights.begin();
             it != mConnectedKeyFrameWeights.end();
             ++it)
        {
            if (it->first == NULL)
            {
                is_id = false;
                ar & is_id;
                continue;
            }
            else
            {
                is_id = true;
                ar & is_id;
                t_nId =  it->first->mnId;
                ar & t_nId;
                ConKfWeight = it->second;
                ar & ConKfWeight;
            }
        }
        // Save each mvpOrderedConnectedKeyFrames
        nItems = mvpOrderedConnectedKeyFrames.size();
        ar & nItems;
        
        for (std::vector<KeyFrame*>::const_iterator it = mvpOrderedConnectedKeyFrames.begin();
             it != mvpOrderedConnectedKeyFrames.end();
             ++it) {
            if (*it == NULL)
            {
                is_id = false;
                ar & is_id;
                continue;
            }
            else
            {
                is_id = true;
                ar & is_id;
                t_nId =  (**it).mnId;
                ar & t_nId;
            }
        }
        // Save Each mvOrderedWeights
#if 0
        
        nItems = mvOrderedWeights.size();
        
        ar & nItems;
        
        for (std::vector<int>::const_iterator it = mvOrderedWeights.begin();
             it != mvOrderedWeights.end();
             ++it)
        {
            ordered_weight =  (*it);
            ar & ordered_weight;
            
        }
#endif
        ar &  const_cast<std::vector<int> &>(mvOrderedWeights);
        
        // Spanning Tree
        ar & const_cast<bool &> (mbFirstConnection);
        
        if (mpParent)
        {
            has_parent = true;
            ar & has_parent;
            ar & mpParent->mnId;
        }
        else
        {
            has_parent = false;
            ar & has_parent;
            //ar & mpParent->mnId;
        }
        // Save each child Frame id
        nItems = mspChildrens.size();
        ar & nItems;
        for (std::set<KeyFrame*>::const_iterator it = mspChildrens.begin(); it != mspChildrens.end(); ++it) {
            if (*it == NULL)
            {
                is_id = false;
                ar & is_id;
                continue;
            }
            else
            {
                is_id = true;
                ar & is_id;
                t_nId =  (**it).mnId;
                //cout << "[" << t_nId <<"]";
                ar & t_nId;
            }
            
        }
        // Save each Loop Edge id
        nItems = mspLoopEdges.size();
        ar & nItems;
        for (std::set<KeyFrame*>::const_iterator it = mspLoopEdges.begin(); it != mspLoopEdges.end(); ++it) {
            if (*it == NULL)
            {
                is_id = false;
                ar & is_id;
                continue;
            }
            else
            {
                is_id = true;
                ar & is_id;
                t_nId =  (**it).mnId;
                //cout << "[" << t_nId <<"]";
                ar & t_nId;
            }
            
        }
        
        ar & const_cast<bool &> (mbNotErase);
        ar & const_cast<bool &> (mbToBeErased);
        ar & const_cast<bool &> (mbBad);
        ar & const_cast<float &> (mHalfBaseline);
        // cout << "{INFO}mvpMapPoints nItems fin"<< endl;
        //cout << "Save Map :  KF stat is : " << endl;
        //t_nId = has_parent?mpParent->mnId:0;
        //cout << "KF mnId = " << mnId << " Parent ID " <<t_nId <<" mspLoopEdges.size() = " << nItems <<endl;
        
        
    }
    
    template<class Archive>
    void KeyFrame::load(Archive & ar, const unsigned int version)
    {
        id_map storer;
        int nItems;bool is_id = false;
        bool has_parent = false;
        long unsigned int t_nId;
        ar & nNextId;
        ar & const_cast<long unsigned int &> (mnId);
        int ConKfWeight = 0;
        //cout << "{INFO}Keyframe Load - " << mnId << endl;
        
        ar & const_cast<long unsigned int &> (mnFrameId);
        ar & const_cast<double &> (mTimeStamp);
        ar & const_cast<int &> (mnGridCols);
        ar & const_cast<int &> (mnGridRows);
        ar & const_cast<float &>  (mfGridElementWidthInv);
        ar & const_cast<float &>  (mfGridElementHeightInv);
        ar & const_cast<long unsigned int &> (mnTrackReferenceForFrame);
        ar & const_cast<long unsigned int &> (mnFuseTargetForKF);
        ar & const_cast<long unsigned int &> (mnBALocalForKF);
        ar & const_cast<long unsigned int &> (mnBAFixedForKF);
        ar & const_cast<long unsigned int &> (mnLoopQuery);
        ar & const_cast<int &> (mnLoopWords);
        ar & const_cast<float &> (mLoopScore);
        ar & const_cast<long unsigned int &> (mnRelocQuery);
        ar & const_cast<int &> (mnRelocWords);
        ar & const_cast<float &> (mRelocScore);
        ar & const_cast<cv::Mat &> (mTcwGBA);
        ar & const_cast<cv::Mat &> (mTcwBefGBA);
        ar & const_cast<long unsigned int &> (mnBAGlobalForKF);
        ar & const_cast<float &> (fx);
        ar & const_cast<float &> (fy);
        ar & const_cast<float &> (cx);
        ar & const_cast<float &> (cy);
        ar & const_cast<float &> (invfx);
        ar & const_cast<float &> (invfy);
        ar & const_cast<float &> (mbf);
        ar & const_cast<float &> (mb);
        ar & const_cast<float &> (mThDepth);
        ar & const_cast<int &> (N);
        ar & const_cast<std::vector<cv::KeyPoint> &> (mvKeys);
        ar & const_cast<std::vector<cv::KeyPoint> &> (mvKeysUn);
        ar & const_cast<std::vector<float> &> (mvuRight);
        ar & const_cast<std::vector<float> &> (mvDepth);
        ar & const_cast<cv::Mat &> (mDescriptors);
        ar & const_cast<cv::Mat &> (mTcp);
        ar & const_cast<int &> (mnScaleLevels);
        ar & const_cast<float &> (mfScaleFactor);
        ar & const_cast<float &> (mfLogScaleFactor);
        ar & const_cast<std::vector<float> &> (mvScaleFactors);
        ar & const_cast<std::vector<float> &> (mvLevelSigma2);
        ar & const_cast<std::vector<float> &> (mvInvLevelSigma2);
        
        ar & const_cast<int &> (mnMinX);
        ar & const_cast<int &> (mnMinY);
        ar & const_cast<int &> (mnMaxX);
        ar & const_cast<int &> (mnMaxY);
        ar & const_cast<cv::Mat &> (mK);
        ar & const_cast<cv::Mat &> (Tcw);
        ar & const_cast<cv::Mat &> (Twc);
        ar & const_cast<cv::Mat &> (Ow);
        ar & const_cast<cv::Mat &> (Cw);
        // Load each map point id
        ar & nItems;
        mvpMapPoints.resize(nItems);
        //mmMapPoints_nId.resize(nItems);
        int j=0;
        for (int i = 0; i < nItems; ++i) {
            
            ar & is_id;
            if (is_id)
            {
                j++;
                ar & t_nId;
                storer.is_valid = true;
                storer.id= t_nId;
                mmMapPoints_nId[i] = storer;
            }
            else
            {
                storer.is_valid = false;
                storer.id= 0;
                mmMapPoints_nId[i] = storer;
            }
        }
        
        //cout << "KF " << mnId <<" valid points = " << j << "invalid points = " << (nItems - j) << endl;
        // Grid
        ar & const_cast<std::vector< std::vector <std::vector<size_t> > > &> (mGrid);
        
        ar & nItems;
        //mConnectedKeyFrameWeights_nId.resize(nItems);
        //mConnectedKeyFrameWeights.resize(nItems);
        for (int i = 0; i < nItems; ++i) {
            
            ar & is_id;
            if (is_id)
            {
                ar & t_nId;
                ar & ConKfWeight;
                mConnectedKeyFrameWeights_nId[t_nId] = ConKfWeight;
            }
            else
            {
                
            }
        }
        
        // Load each mvpOrderedConnectedKeyFrames id
        ar & nItems;
        j = 0;
        //mvpOrderedConnectedKeyFrames.resize(nItems);
        //mvpOrderedConnectedKeyFrames_nId.resize(nItems);
        for (int i = 0; i < nItems; ++i) {
            ar & is_id;
            if (is_id)
            {
                j++;
                ar & t_nId;
                storer.is_valid = true;
                storer.id= t_nId;
                mvpOrderedConnectedKeyFrames_nId[i] = storer;
            }
            else
            {
                storer.is_valid = false;
                storer.id= 0;
                mvpOrderedConnectedKeyFrames_nId[i] = storer;
            }
        }
        // Load each mvOrderedWeights
        ar & const_cast<std::vector<int> &>(mvOrderedWeights);
        
        ar & const_cast<bool &> (mbFirstConnection);
        
        // Spanning Tree
        ar & has_parent;
        if (has_parent)
        {
            mparent_KfId_map.is_valid = true;
            ar & mparent_KfId_map.id;
        }
        else
        {
            mparent_KfId_map.is_valid = false;
            mparent_KfId_map.id = 0;
        }
        // load each child Frame id
        ar & nItems;
        //mspChildrens.resize(nItems);
        //mmChildrens_nId.resize(nItems);
        
        for (int i = 0; i < nItems; ++i) {
            
            ar & is_id;
            if (is_id)
            {
                ar & t_nId;
                storer.is_valid = true;
                storer.id= t_nId;
                mmChildrens_nId[i] = storer;
            }
            else
            {
                storer.is_valid = false;
                storer.id= 0;
                mmChildrens_nId[i] = storer;
            }
        }
        
        // Load each Loop Edge Frame id
        ar & nItems;
        //mspLoopEdges.resize(nItems);
        //mmLoopEdges_nId.resize(nItems);
        
        for (int i = 0; i < nItems; ++i) {
            
            ar & is_id;
            if (is_id)
            {
                ar & t_nId;
                storer.is_valid = true;
                storer.id= t_nId;
                mmLoopEdges_nId[i] = storer;
            }
            else
            {
                storer.is_valid = false;
                storer.id= 0;
                mmLoopEdges_nId[i] = storer;
            }
        }
        
        ar & const_cast<bool &> (mbNotErase);
        ar & const_cast<bool &> (mbToBeErased);
        ar & const_cast<bool &> (mbBad);
        ar & const_cast<float &> (mHalfBaseline);
        //cout << "Load Map :  KF stat is : " << endl;
        //cout << "KF mnId = " << mnId << " Parent ID " <<mparent_KfId_map.id <<" mspLoopEdges.size() = " << nItems <<endl;
        
    }
    
    
    // Explicit template instantiation
    template void KeyFrame::save<boost::archive::binary_oarchive>(
                                                                  boost::archive::binary_oarchive &,
                                                                  const unsigned int) const;
    template void KeyFrame::save<boost::archive::binary_iarchive>(
                                                                  boost::archive::binary_iarchive &,
                                                                  const unsigned int) const;
    template void KeyFrame::load<boost::archive::binary_oarchive>(
                                                                  boost::archive::binary_oarchive &,
                                                                  const unsigned int);
    template void KeyFrame::load<boost::archive::binary_iarchive>(
                                                                  boost::archive::binary_iarchive &,
                                                                  const unsigned int);
    
    void KeyFrame::SetMapPoints(std::vector<MapPoint*> spMapPoints)
    {
        // We assume the mvpMapPoints_nId list has been initialized and contains the Map point IDS
        // With nid, Search the KetFrame List and populate mvpMapPoints
        long unsigned int id;
        bool is_valid = false;
        bool mapp_found = false;
        
        int j = 0, ctr = 0;
        for (std::map<long unsigned int,id_map>::iterator it = mmMapPoints_nId.begin();
             it != mmMapPoints_nId.end();
             j++,++it)
        {
            is_valid = it->second.is_valid;
            if (!is_valid)
            {
                //j--;
                //continue;
                mvpMapPoints[j] = static_cast<MapPoint*>(NULL);
            }
            else
            {
                id = it->second.id;
                //cout << "pushing a map point to mvp mappoint" << endl;
                mapp_found = false;
                for(std::vector<MapPoint*>::iterator mit=spMapPoints.begin(); mit !=spMapPoints.end(); mit++)
                {
                    MapPoint* pMp = *mit;
                    
                    if(id == pMp->mnId)
                    {
                        ctr ++;
                        mvpMapPoints[j] = pMp;
                        mapp_found = true;
                        break;
                    }
                }
                if (mapp_found == false)
                {
                    cout << " map point [" << id <<"] not found in KF " << mnId << endl;
                    mvpMapPoints[j] = static_cast<MapPoint*>(NULL);
                }
                
            }
            
        }
    }
    
    void KeyFrame::SetSpanningTree(std::vector<KeyFrame*> vpKeyFrames)
    {
        // We assume the mvpMapPoints_nId list has been initialized and contains the Map point IDS
        // With nid, Search the KetFrame List and populate mvpMapPoints
        long unsigned int id;
        bool is_valid = false;
        bool kf_found = false;
        
        int j = 0, ctr = 0;
        // Search Parent
        if (mparent_KfId_map.is_valid)
        {
            for(std::vector<KeyFrame*>::iterator mit=vpKeyFrames.begin(); mit !=vpKeyFrames.end(); mit++)
            {
                KeyFrame* pKf = *mit;
                id = pKf->mnId;
                //if (mnId == 10 && 964 == id)
                //cout << "[" << pMp->mnId <<"]";
                if(id == mparent_KfId_map.id)
                {
                    ctr ++;
                    mpParent = pKf;
                    kf_found = true;
                    break;
                }
            }
            if (kf_found == false)
            {
                cout << endl << "Parent KF [" << mparent_KfId_map.id <<"] not found for KF " << mnId << endl;
                //mpParent = new KeyFrame();
                //mpParent->mbBad = true;
                mpParent = static_cast<KeyFrame*>(NULL);
            }
        }
        // Search Child
        kf_found = false;
        j = 0; ctr = 0;
        is_valid = false;
        for (std::map<long unsigned int,id_map>::iterator it = mmChildrens_nId.begin();
             it != mmChildrens_nId.end();
             j++,++it)
        {
            is_valid = it->second.is_valid;
            if (!is_valid)
            {
                //j--;
                continue;
                //mspChildrens.insert(NULL);
            }
            
            else
            {
                id = it->second.id;
                
                kf_found = false;
                for(std::vector<KeyFrame*>::iterator mit=vpKeyFrames.begin(); mit !=vpKeyFrames.end(); mit++)
                {
                    KeyFrame* pKf = *mit;
                    //if (mnId == 10 && 964 == id)
                    //cout << "[" << pMp->mnId <<"]";
                    if(id == pKf->mnId)
                    {
                        ctr ++;
                        mspChildrens.insert(pKf);
                        kf_found = true;
                        break;
                    }
                }
                if (kf_found == false)
                    cout << endl << "Child [" << id <<"] not found for KF " << mnId << endl;
                
            }
            
        }
        
        // Search Loop Edges
        kf_found = false;
        j = 0; ctr = 0;
        is_valid = false;
        for (std::map<long unsigned int,id_map>::iterator it = mmLoopEdges_nId.begin();
             it != mmLoopEdges_nId.end();
             j++,++it)
        {
            is_valid = it->second.is_valid;
            
            
            if (!is_valid)
            {
                //j--;
                continue;
                //mspLoopEdges.insert(NULL);
            }
            
            else
            {
                id = it->second.id;
                
                kf_found = false;
                for(std::vector<KeyFrame*>::iterator mit=vpKeyFrames.begin(); mit !=vpKeyFrames.end(); mit++)
                {
                    KeyFrame* pKf = *mit;
                    
                    if(id == pKf->mnId)
                    {
                        ctr ++;
                        mspLoopEdges.insert(pKf);
                        kf_found = true;
                        break;
                    }
                }
                if (kf_found == false)
                    cout << endl << "Loop Edge [" << id <<"] not found for KF " << mnId << endl;
                
            }
            
        }
        
    }
    void KeyFrame::SetGridParams(std::vector<KeyFrame*> vpKeyFrames)
    {
        long unsigned int id; int weight;
        bool Kf_found = false;
        //cout << "KF" << mnId <<" valid indexes-" << endl;
        int j = 0;
        int ctr = 0;
        bool is_valid = false;
        
        
        // Set up mConnectedKeyFrameWeights
        for (map<long unsigned int, int>::iterator it = mConnectedKeyFrameWeights_nId.begin();
             it != mConnectedKeyFrameWeights_nId.end();
             j++,++it)
        {
            id = it->first;
            weight = it->second;
            {
                
                for(std::vector<KeyFrame*>::iterator mit=vpKeyFrames.begin(); mit !=vpKeyFrames.end(); mit++)
                {
                    KeyFrame* pKf = *mit;
                    
                    if(id == pKf->mnId)
                    {
                        mConnectedKeyFrameWeights[pKf] = weight;
                        break;
                    }
                }
                
            }
        }
        
        // Set up mvpOrderedConnectedKeyFrames
        j = 0;
        for (std::map<long unsigned int,id_map>::iterator it = mvpOrderedConnectedKeyFrames_nId.begin();
             it != mvpOrderedConnectedKeyFrames_nId.end();
             ++it)
        {
            is_valid = it->second.is_valid;
            if (!is_valid)
            {
                continue;
                ;//mvpOrderedConnectedKeyFrames[j] = NULL;
            }
            else
            {
                id = it->second.id;
                
                Kf_found = false;
                for(std::vector<KeyFrame*>::iterator mit=vpKeyFrames.begin(); mit !=vpKeyFrames.end(); mit++)
                {
                    KeyFrame* pKf = *mit;
                    
                    if(id == pKf->mnId)
                    {
                        ctr ++;
                        mvpOrderedConnectedKeyFrames.push_back(pKf);
                        Kf_found = true;
                        break;
                    }
                }
                if (Kf_found == false)
                {
                    //cout << "[" << id <<"] not found in KF " << mnId << endl;
                    
                }
                
            }
            
        }
    }
    
    void KeyFrame::SetMap(Map* map)
    {
        mpMap = map;
    }
    
    void KeyFrame::SetKeyFrameDatabase(KeyFrameDatabase* pKeyFrameDB)
    {
        mpKeyFrameDB = pKeyFrameDB;
    }
    
    void KeyFrame::SetORBvocabulary(ORBVocabulary* pORBvocabulary)
    {
        mpORBvocabulary = pORBvocabulary;
    }

    
    
    //
    
    
    boost::property_tree::ptree* KeyFrame::propertyTreeFromKeyframeId() {
        boost::property_tree::ptree  * keyframeTreeReturn = new boost::property_tree::ptree;
        keyframeTreeReturn->put("mnId", mnId);
        return keyframeTreeReturn;
    }
                                                                     
    std::vector<KeyFrame*> KeyFrame::keyFrameFromPropertyTree(boost::property_tree::ptree keyFrameTree) {
//        BOOST_FOREACH( boost::property_tree::ptree::value_type const& node, keyFrameTree.get_child( "KeyFrames" ) )
//        {
//            boost::property_tree::ptree subtree = node.second;
//
//            if ( node.first == "KeyFrame" )
//            {
//                long unsigned int nNextId = subtree.get<long unsigned int>("nNextId");
//                long unsigned int mnId = subtree.get<long unsigned int>("mnId");
//                long unsigned int mnFrameId = subtree.get<long unsigned int>("mnFrameId");
//                double mTimeStamp = subtree.get<double>("mTimeStamp");
//                int mnGridCols = subtree.get<int>("mnGridCols");
//                int mnGridRows = subtree.get<int>("mnGridRows");
//                float mfGridElementWidthInv = subtree.get<float>("mfGridElementWidthInv");
//                float mfGridElementHeightInv = subtree.get<float>("mfGridElementHeightInv");
//                long unsigned int mnTrackReferenceForFrame = subtree.get<long unsigned int>("mnTrackReferenceForFrame");
//                long unsigned int mnFuseTargetForKF = subtree.get<long unsigned int>("mnFuseTargetForKF");
//                long unsigned int mnBALocalForKF = subtree.get<long unsigned int>("mnBALocalForKF");
//                long unsigned int mnBAFixedForKF = subtree.get<long unsigned int>("mnBAFixedForKF");
//                long unsigned int mnLoopQuery = subtree.get<long unsigned int>("mnLoopQuery");
//                int mnLoopWords = subtree.get<int>("mnLoopWords");
//                float mLoopScore = subtree.get<float>("mLoopScore");
//                long unsigned int mnRelocQuery = subtree.get<long unsigned int>("mnRelocQuery");
//                int mnRelocWords = subtree.get<int>("mnRelocWords");
//                float mRelocScore = subtree.get<float>("mRelocScore");
//
//                cv::Mat mTcwGBA = KeyFrame::matrixFromPropertyTree(subtree.get_child("mTcwGBA"));
//                cv::Mat mTcwBefGBA = KeyFrame::matrixFromPropertyTree(subtree.get_child("mTcwBefGBA"));
//
//                long unsigned int mnBAGlobalForKF = subtree.get<long unsigned int>("mnBAGlobalForKF");
//                float fx = subtree.get<float>("fx");
//                float fy = subtree.get<float>("fy");
//                float cx = subtree.get<float>("cx");
//                float cy = subtree.get<float>("cy");
//                float invfx = subtree.get<float>("invfx");
//                float invfy = subtree.get<float>("invfy");
//                float mbf = subtree.get<float>("mbf");
//                float mb = subtree.get<float>("mb");
//                float mThDepth = subtree.get<float>("mThDepth");
//                int N = subtree.get<int>("N");
//
//                std::vector<cv::KeyPoint> mvKeys = std::vector<cv::KeyPoint>();
//
//                BOOST_FOREACH( boost::property_tree::ptree::value_type const& keysNode, subtree.get_child( "mvKeys" ) ) {
//                    float x = keysNode.second.get<float>("x");
//                    float y = keysNode.second.get<float>("y");
//
//                    cv::KeyPoint keyPoint = cv::KeyPoint();
//                    keyPoint.pt.x = x;
//                    keyPoint.pt.y = y;
//
//                    mvKeys.push_back(keyPoint);
//                }
//
//                std::vector<cv::KeyPoint> mvKeysUn = std::vector<cv::KeyPoint>();
//
//                BOOST_FOREACH( boost::property_tree::ptree::value_type const& keysNode, subtree.get_child( "mvKeysUn" ) ) {
//                    float x = keysNode.second.get<float>("x");
//                    float y = keysNode.second.get<float>("y");
//
//                    cv::KeyPoint keyPoint = cv::KeyPoint();
//                    keyPoint.pt.x = x;
//                    keyPoint.pt.y = y;
//
//                    mvKeysUn.push_back(keyPoint);
//                }
//
//                std::vector<float> mvuRight = std::vector<float>();
//
//                BOOST_FOREACH( boost::property_tree::ptree::value_type const& keysNode, subtree.get_child( "mvuRight" ) ) {
//                    float floatValue = keysNode.second.get<float>("float");
//
//                    mvuRight.push_back(floatValue);
//                }
//
//                std::vector<float> mvDepth = std::vector<float>();
//
//                BOOST_FOREACH( boost::property_tree::ptree::value_type const& keysNode, subtree.get_child( "mvDepth" ) ) {
//                    float floatValue = keysNode.second.get<float>("float");
//
//                    mvDepth.push_back(floatValue);
//                }
//
//                cv::Mat mDescriptors = KeyFrame::matrixFromPropertyTree(subtree.get_child("mDescriptors"));
//
//
//                DBoW2::BowVector mBowVec = KeyFrame::dbow2FromPropertyTree(subtree.get_child("mBowVec"));
//                DBoW2::BowVector mFeatVec = KeyFrame::dbow2FromPropertyTree(subtree.get_child("mFeatVec"));
//
//                cv::Mat mTcp = KeyFrame::matrixFromPropertyTree(subtree.get_child("mTcp"));
//
//                int mnScaleLevels = subtree.get<int>("mnScaleLevels");
//                float mfScaleFactor = subtree.get<int>("mfScaleFactor");
//                float mfLogScaleFactor = subtree.get<int>("mfLogScaleFactor");
//                std::vector<float> mvScaleFactors = subtree.get<std::vector<float>>("mvScaleFactors");
//                std::vector<float> mvLevelSigma2 = subtree.get<std::vector<float>>("mvLevelSigma2");
//                std::vector<float> mvInvLevelSigma2 = subtree.get<std::vector<float>>("mvInvLevelSigma2");
////
////                int mnMinX = subtree.get<int>("mnMinX");
////                int mnMinY = subtree.get<int>("mnMinY");
////                int mnMaxX = subtree.get<int>("mnMaxX");
////                int mnMaxY = subtree.get<int>("mnMaxY");
////
////
//////                cv::Mat mK = KeyFrame::matrixFromPropertyTree(subtree.get_child("mK"));
////
////
////                cv::Mat Tcw = KeyFrame::matrixFromPropertyTree(subtree.get_child("Tcw"));
////                cv::Mat Twc = KeyFrame::matrixFromPropertyTree(subtree.get_child("Twc"));
////                cv::Mat Ow = KeyFrame::matrixFromPropertyTree(subtree.get_child("Ow"));
////                cv::Mat Cw = KeyFrame::matrixFromPropertyTree(subtree.get_child("Cw"));
//
//
//                //TODO Make method for loading in the map points.
////                std::vector<MapPoint*> mvpMapPoints = std::vector<MapPoint*>();
////
////                BOOST_FOREACH( boost::property_tree::ptree::value_type const& keysNode, subtree.get_child( "mvpMapPoints" ) ) {
////                    float x = keysNode.second.get<float>("x");
////                    float y = keysNode.second.get<float>("y");
////                    float z = keysNode.second.get<float>("z");
////
////                    MapPoint* mapPoint = new MapPoint();
////                    mapPoint.pt.x = x;
////                    mapPoint.pt.y = y;
////                    mapPoint.pt.z = z;
////
////                    mvpMapPoints.push_back(mapPoint);
////                }
//
//                //TODO KeyFrameDatabase* mpKeyFrameDB when import is done correctly
//                //TODO ORBVocabulary* mpORBvocabulary when import is done correctly
//
//                std::vector< std::vector <std::vector<size_t> > > mGrid = std::vector< std::vector <std::vector<size_t> > >();
//
//                std::vector<int> mvOrderedWeights = std::vector<int>();
//
//                BOOST_FOREACH( boost::property_tree::ptree::value_type const& keysNode, subtree.get_child( "mvOrderedWeights" ) ) {
//
//                    //mvDepth.push_back(floatValue);
//
//                }
//
////
////                KeyFrame * keyFrame = new KeyFrame(nNextId, mnId, mnFrameId, mTimeStamp, mnGridCols, mnGridRows, mfGridElementWidthInv, mfGridElementHeightInv, mnTrackReferenceForFrame, mnFuseTargetForKF, mnBALocalForKF, mnBAFixedForKF, mnLoopQuery, mnLoopWords, mLoopScore, mnRelocQuery, mnRelocWords, mRelocScore, mTcwGBA, mTcwBefGBA, mnBAGlobalForKF, fx, ty, cx, cy, invfx, invfy, mbf, mb, mThDepth, N);
//
//
//                //                {
//                //                    std::string label = v.first;
//                //
//                //                    if ( label != "<xmlattr>" )
//                //                    {
//                //                        std::string value = subtree.get<std::string>( label );
//                //                        std::cout << label << ":  " << value << std::endl;
//                //                    }
//                //                }
//                //                std::cout << std::endl;
//            }
//        }
    }
    
    boost::property_tree::ptree* KeyFrame::propertyTreeFromKeyframe( std::map<KeyFrame* , boost::property_tree::ptree*> *referenceKeyFrames) {
        
        //KeyFrame reference
        //If KeyFrame is already in the XML store the mnId
        std::map<KeyFrame* , boost::property_tree::ptree*>::iterator it = referenceKeyFrames->find(this);
        if(it != referenceKeyFrames->end())
        {
            boost::property_tree::ptree  * keyframeTreeReturn = new boost::property_tree::ptree;
            keyframeTreeReturn->put("mnId", mnId);
            return keyframeTreeReturn;
        }

        
        boost::property_tree::ptree  * keyframeTree = new boost::property_tree::ptree;
        referenceKeyFrames->insert( std::pair<KeyFrame* , boost::property_tree::ptree*>(this,keyframeTree) );

        keyframeTree->put("nNextId", nNextId);
        keyframeTree->put("mnId", mnId);
        keyframeTree->put("mnFrameId", const_cast<long unsigned int &>(mnFrameId) );
        keyframeTree->put("mTimeStamp", const_cast<double &>(mTimeStamp) );
        
        keyframeTree->put("mnGridCols", const_cast<int &>(mnGridCols) );
        keyframeTree->put("mnGridRows", const_cast<int &>(mnGridRows) );
        
        keyframeTree->put("mfGridElementWidthInv", const_cast<float &>(mfGridElementWidthInv));
        keyframeTree->put("mfGridElementHeightInv", const_cast<float &>(mfGridElementHeightInv));
        
        keyframeTree->put("mnTrackReferenceForFrame", (mnTrackReferenceForFrame));
        keyframeTree->put("mnFuseTargetForKF",(mnFuseTargetForKF));
        
        keyframeTree->put("mnBALocalForKF",(mnBALocalForKF));
        keyframeTree->put("mnBAFixedForKF",(mnBAFixedForKF));
        
        keyframeTree->put("mnLoopQuery",(mnLoopQuery));
        keyframeTree->put("mnLoopWords",(mnLoopWords));
        keyframeTree->put("mLoopScore",(mLoopScore));
        keyframeTree->put("mnRelocQuery",(mnRelocQuery));
        keyframeTree->put("mnRelocWords",(mnRelocWords));
        keyframeTree->put("mRelocScore",(mRelocScore));
        
        keyframeTree->add_child("mTcwGBA",  propertyTreeFromMatrix(mTcwGBA) );
        keyframeTree->add_child("mTcwBefGBA",  propertyTreeFromMatrix(mTcwBefGBA) );
        keyframeTree->put("mnBAGlobalForKF", (mnBAGlobalForKF) );
        
        keyframeTree->put("mTcwGBA",(mTcwGBA));
        keyframeTree->put("mTcwBefGBA",(mTcwBefGBA));
        keyframeTree->put("mnBAGlobalForKF",(mnBAGlobalForKF));
        
        keyframeTree->put("fx",(fx));
        keyframeTree->put("fy",(fy));
        keyframeTree->put("cx",(cx));
        keyframeTree->put("cy",(cy));
        
        keyframeTree->put("invfx",(invfx));
        keyframeTree->put("invfy",(invfy));
        keyframeTree->put("mbf",(mbf));
        
        keyframeTree->put("mb",(mb));
        keyframeTree->put("mThDepth",(mThDepth));
        
        keyframeTree->put("N",(const_cast<int &>(N)));
        
        
        //keypoints mvKeys
        boost::property_tree::ptree mvKeysTree;
        
        for(auto key : mvKeys) {
            boost::property_tree::ptree keypointTree;
            keypointTree.put("x", key.pt.x  );
            keypointTree.put("y", key.pt.y  );
            mvKeysTree.push_back(boost::property_tree::ptree::value_type("KeyPoint", keypointTree));
        }
        
        //add keypoints to mvKeys tree
        keyframeTree->add_child("mvKeys", mvKeysTree);
        
        
        //keypoints mvKeysUn
        boost::property_tree::ptree mvKeysUnTree;
        
        for(auto key : mvKeysUn) {
            boost::property_tree::ptree keypointTree;
            keypointTree.put("x", key.pt.x  );
            keypointTree.put("y", key.pt.y  );
            mvKeysUnTree.push_back(boost::property_tree::ptree::value_type("KeyPoint", keypointTree));
        }
        
        //add keypoints to mvKeys tree
        keyframeTree->add_child("mvKeysUn", mvKeysUnTree);
        
        
        
        //mvuRight
        boost::property_tree::ptree mvuRightTree;
        for(auto floatValue : mvuRight) {
            boost::property_tree::ptree localValue;
            localValue.put_value(floatValue);
            mvuRightTree.push_back(boost::property_tree::ptree::value_type("float", localValue));
        }
        
        //add float to mvuRight tree
        keyframeTree->add_child("mvuRight", mvuRightTree);
        
        
        //mvDepth
        boost::property_tree::ptree mvDepthTree;
        for(auto floatValue : mvDepth) {
            boost::property_tree::ptree localValue;
            localValue.put_value(floatValue);
            mvDepthTree.push_back(boost::property_tree::ptree::value_type("float", localValue));
        }
        
        //add float to mvuRight tree
        keyframeTree->add_child("mvDepth", mvDepthTree);
        
        
        
        
        //dDescriptors
        keyframeTree->add_child("mDescriptors",  propertyTreeFromMatrix(mDescriptors) );

        // Bow
        keyframeTree->add_child("mBowVec",  propertyTreeFromBowVector(mBowVec) );
        keyframeTree->add_child("mFeatVec",  propertyTreeFromFeatureVector(mFeatVec) );
        
        // Pose relative to parent
         keyframeTree->add_child("mFeatVec",  propertyTreeFromMatrix(mTcp) );
        
        
        
        
        // Scale related
        keyframeTree->put("mnScaleLevels",(const_cast<int &>(mnScaleLevels)));
        keyframeTree->put("mfScaleFactor", const_cast<float &>(mfScaleFactor));
        keyframeTree->put("mfLogScaleFactor", const_cast<float &>(mfLogScaleFactor));
        
        
        //mvScaleFactors
        boost::property_tree::ptree mvScaleFactorsTree;
        for(auto floatValue : mvScaleFactors) {
            boost::property_tree::ptree localValue;
            localValue.put_value(const_cast<float &>(floatValue));
            mvScaleFactorsTree.push_back(boost::property_tree::ptree::value_type("float", localValue));
        }
        
        //add float to mvuRight tree
        keyframeTree->add_child("mvScaleFactors", mvScaleFactorsTree);
        
        
        
        //mvLevelSigma2
        boost::property_tree::ptree mvLevelSigma2Tree;
        for(auto floatValue : mvLevelSigma2) {
            boost::property_tree::ptree localValue;
            localValue.put_value(const_cast<float &>(floatValue));
            mvLevelSigma2Tree.push_back(boost::property_tree::ptree::value_type("float", localValue));
        }
        
        //add float to mvuRight tree
        keyframeTree->add_child("mvLevelSigma2", mvLevelSigma2Tree);
        
        
        
        //mvLevelSigma2
        boost::property_tree::ptree mvInvLevelSigma2Tree;
        for(auto floatValue : mvInvLevelSigma2) {
            boost::property_tree::ptree localValue;
            localValue.put_value(const_cast<float &>(floatValue));
            mvInvLevelSigma2Tree.push_back(boost::property_tree::ptree::value_type("float", localValue));
        }
        
        //add float to mvuRight tree
        keyframeTree->add_child("mvInvLevelSigma2", mvInvLevelSigma2Tree);
        
        
        
        // Image bounds and calibration
        keyframeTree->put("mnMinX", const_cast<int &>(mnMinX));
        keyframeTree->put("mnMinY",  const_cast<int &>(mnMinY)  );
        keyframeTree->put("mnMaxX",  const_cast<int &>(mnMaxX) );
        keyframeTree->put("mnMaxY",  const_cast<int &>(mnMaxY) );
        
        
        keyframeTree->add_child("mK",  propertyTreeFromMatrix(mK) );

        // mutex needed vars, but don't lock mutex in the save/load procedure
        {
            unique_lock<mutex> lock_pose(mMutexPose);
             keyframeTree->add_child("Tcw",  propertyTreeFromMatrix(Tcw) );
             keyframeTree->add_child("Twc",  propertyTreeFromMatrix(Twc) );
             keyframeTree->add_child("Ow",  propertyTreeFromMatrix(Ow) );
             keyframeTree->add_child("Cw",  propertyTreeFromMatrix(Cw) );
        }
        {
            unique_lock<mutex> lock_feature(mMutexFeatures);
            
            
            
            //mappoints
            
            //TODO: needs to save size of array ??
            boost::property_tree::ptree mvpMapPointsTree;
            for(auto mapPoint : mvpMapPoints) {
                if (mapPoint != nullptr) {
                    boost::property_tree::ptree mapPointLocal;
                    mapPointLocal.put("x", mapPoint->GetWorldPos().at<float>(0)  );
                    mapPointLocal.put("y", mapPoint->GetWorldPos().at<float>(1) );
                    mapPointLocal.put("z", mapPoint->GetWorldPos().at<float>(2) );
                    
                    mvpMapPointsTree.push_back(boost::property_tree::ptree::value_type("mappoint", mapPointLocal));
                }
                
            }
            
            //add mappoints to map tree
            keyframeTree->add_child("mvpMapPoints", mvpMapPointsTree);
        }
        
        
//        // BoW
//        ar & mpKeyFrameDB;
//        // mpORBvocabulary restore elsewhere(see SetORBvocab)
        {
//            // Grid related
            unique_lock<mutex> lock_connection(mMutexConnections);

        //mGrid
         boost::property_tree::ptree baseArrayTree;
            for (int i = 0; i < mGrid.size(); i++) {
                boost::property_tree::ptree arrayTree;
                for (int j = 0; j < (mGrid[i].size()); j++) {
                     boost::property_tree::ptree innerArrayTree;
                     for (int k = 0; k < (mGrid[i][j].size()); k++) {
                         if (!mGrid[i][j][k]) {
                             boost::property_tree::ptree localValue;
                             localValue.put_value( (mGrid[i][j][k]) );
                             innerArrayTree.push_back(boost::property_tree::ptree::value_type("float", localValue));
                         }
                     }
                    arrayTree.push_back(boost::property_tree::ptree::value_type("array", innerArrayTree));
                }
                baseArrayTree.push_back(boost::property_tree::ptree::value_type("baseArray", arrayTree));
            }
        keyframeTree->push_back(boost::property_tree::ptree::value_type("mGrid", baseArrayTree));
        
        
        //mConnectedKeyFrameWeights
        boost::property_tree::ptree mConnectedKeyFrameWeightsTree;
        for (auto const& valueMConnectedKeyFrameWeights : mConnectedKeyFrameWeights)
        {
            boost::property_tree::ptree localValue;
            localValue.push_back(boost::property_tree::ptree::value_type("KeyFrame",  *valueMConnectedKeyFrameWeights.first->propertyTreeFromKeyframeId()  ) );
            localValue.put("weight",  valueMConnectedKeyFrameWeights.second );
            mConnectedKeyFrameWeightsTree.push_back(boost::property_tree::ptree::value_type("ConnectedKeyFrameWeight", localValue));
        }
        keyframeTree->push_back(boost::property_tree::ptree::value_type("mConnectedKeyFrameWeights", mConnectedKeyFrameWeightsTree));
        
        
        //mvpOrderedConnectedKeyFrames
        boost::property_tree::ptree mvpOrderedConnectedKeyFramesTree;
        for (auto const& valuemvpOrderedConnectedKeyFrames : mvpOrderedConnectedKeyFrames)
        {
            boost::property_tree::ptree localValue;
            localValue.push_back(boost::property_tree::ptree::value_type("KeyFrame",  *valuemvpOrderedConnectedKeyFrames->propertyTreeFromKeyframeId()  ) );
            mvpOrderedConnectedKeyFramesTree.push_back(boost::property_tree::ptree::value_type("mvpOrderedConnectedKeyFrame", localValue));
        }
        keyframeTree->push_back(boost::property_tree::ptree::value_type("mvpOrderedConnectedKeyFrames", mvpOrderedConnectedKeyFramesTree));
        
        
        //mvOrderedWeights
        boost::property_tree::ptree  mvOrderedWeightsTree;
        for (auto const& mvOrderedWeightsValue : mvOrderedWeights)
        {
            boost::property_tree::ptree localValue;
            localValue.put("int",  mvOrderedWeightsValue  );
            mvOrderedWeightsTree.push_back(boost::property_tree::ptree::value_type("mvOrderedWeight", localValue));
        }
        keyframeTree->push_back(boost::property_tree::ptree::value_type("mvOrderedWeights", mvOrderedWeightsTree));
        
        // Spanning Tree and Loop Edges
            keyframeTree->put("mbFirstConnection",(mbFirstConnection));
            
            if (mpParent == nullptr) {
                boost::property_tree::ptree localValue;
                localValue.put("mspLoopEdge", "NULL");
                keyframeTree->push_back(boost::property_tree::ptree::value_type("mpParent.KeyFrame",(   localValue )));
            } else {
                keyframeTree->push_back(boost::property_tree::ptree::value_type( "mpParent.KeyFrame",*(mpParent->propertyTreeFromKeyframeId()  ) ) ) ;
            }
            
            
            //mspChildrens
            boost::property_tree::ptree mspChildrensTree;
            for (auto const& mspChildrensValue : mspChildrens)
            {
                boost::property_tree::ptree localValue;
                localValue.push_back(boost::property_tree::ptree::value_type("KeyFrame",  *mspChildrensValue->propertyTreeFromKeyframeId()  ) );
                mspChildrensTree.push_back(boost::property_tree::ptree::value_type("mspChildren", localValue));
            }
            keyframeTree->push_back(boost::property_tree::ptree::value_type("mspChildrens", mspChildrensTree));
            
            
            //mspLoopEdges
            boost::property_tree::ptree mspLoopEdgesTree;
            for (auto const& mspLoopEdgesValue : mspLoopEdges)
            {
                boost::property_tree::ptree localValue;
                localValue.push_back(boost::property_tree::ptree::value_type("KeyFrame",  *mspLoopEdgesValue->propertyTreeFromKeyframeId()  ) );
                mspLoopEdgesTree.push_back(boost::property_tree::ptree::value_type("mspLoopEdge", localValue));
            }
            keyframeTree->push_back(boost::property_tree::ptree::value_type("mspLoopEdges", mspLoopEdgesTree));
            
            
            // Bad flags
            keyframeTree->put("mbNotErase",(mbNotErase));
            keyframeTree->put("mbToBeErased",(mbToBeErased));
            keyframeTree->put("mbBad",(mbBad));
            keyframeTree->put("mHalfBaseline",(mHalfBaseline));
        }
        
        
        //TODO: Map Points
//        ar & mpMap;
        
        return keyframeTree;
    }
    
    DBoW2::BowVector KeyFrame::dbow2FromPropertyTree(boost::property_tree::ptree const tree) {
        
        DBoW2::BowVector vectorArray = DBoW2::BowVector();
        BOOST_FOREACH( boost::property_tree::ptree::value_type const& DataNode,  tree.get_child( "map" ) ) {
            vectorArray.addWeight(DataNode.second.get<unsigned int>("int"), DataNode.second.get<double>("double"));
        }
        return vectorArray;
    }
    
    
    cv::Mat KeyFrame::matrixFromPropertyTree(boost::property_tree::ptree const tree) {
        int rows = tree.get<int>("rows");
        int cols = tree.get<int>("cols");
        
        float valueArray[rows * cols];
    
        int itValue = 0;
        BOOST_FOREACH( boost::property_tree::ptree::value_type const& DataNode,  tree.get_child( "Data" ) ) {
            valueArray[itValue] = DataNode.second.get<float>("float");
            itValue++;
        }
        
        cv::Mat matrixTree  = cv::Mat(rows,cols,CV_32F, valueArray);
        return matrixTree;
    }
    
    boost::property_tree::ptree  KeyFrame::propertyTreeFromMatrix(cv::Mat const matrix) {
        boost::property_tree::ptree matrixTree;
        
        matrixTree.put("rows", matrix.rows );
        matrixTree.put("cols", matrix.cols );
        
        boost::property_tree::ptree valuesTree;
           
//        cv::MatConstIterator_<double> _it = matrix.begin<double>();
//        for(;_it!=matrix.end<double>(); _it++){
        for (float &f : cv::Mat_<float>(matrix)) {
            boost::property_tree::ptree localValue;
            localValue.put_value(f);
            valuesTree.push_back(boost::property_tree::ptree::value_type("float", localValue));
        }
        matrixTree.push_back(boost::property_tree::ptree::value_type("Data", valuesTree));
        
        return matrixTree;
    }
    
    boost::property_tree::ptree  KeyFrame::propertyTreeFromBowVector(DBoW2::BowVector const vector) {
        boost::property_tree::ptree matrixTree;

        boost::property_tree::ptree valuesTree;
        
        for ( const auto bowValue : vector )
        {
            boost::property_tree::ptree localValue;
            localValue.put_value(bowValue.first);
            
            boost::property_tree::ptree localValueDouble;
            localValueDouble.put_value(bowValue.second);
            
            valuesTree.push_back(boost::property_tree::ptree::value_type("int",  localValue));
            valuesTree.push_back(boost::property_tree::ptree::value_type("double", localValueDouble));
        }

        matrixTree.push_back(boost::property_tree::ptree::value_type("map", valuesTree));
        
        return matrixTree;
    }
    
    
    boost::property_tree::ptree  KeyFrame::propertyTreeFromFeatureVector(DBoW2::FeatureVector const vector) {
        boost::property_tree::ptree matrixTree;
        
        boost::property_tree::ptree valuesTree;
        
        for ( const  auto  bowValue : vector ) {
            boost::property_tree::ptree localValue;
            localValue.put_value(bowValue.first);
            
            boost::property_tree::ptree valuesTreeInner;
            for ( const int  bowArrayValue : bowValue.second ) {
                boost::property_tree::ptree localValueDouble;
                localValueDouble.put_value(bowArrayValue);
                
                valuesTreeInner.push_back(boost::property_tree::ptree::value_type("intVector",  localValueDouble));
            }
           
            
            valuesTree.push_back(boost::property_tree::ptree::value_type("int",  localValue));
            valuesTree.push_back(boost::property_tree::ptree::value_type("array", valuesTreeInner));
        }
        
        matrixTree.push_back(boost::property_tree::ptree::value_type("map", valuesTree));
        
        return matrixTree;
    }
    

}
//namespace ORB_SLAM
