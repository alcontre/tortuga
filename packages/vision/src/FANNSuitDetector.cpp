/*
 * Copyright (C) 2008 Robotics at Maryland
 * Copyright (C) 2008 David Love <loved@umd.edu>
 * All rights reserved.
 *
 * Author: David Love <loved@umd.edu>
 * File:  packages/vision/src/FANNSuiteDetector.cpp
 */

// header for this class
#include "vision/include/FANNSuitDetector.hpp"

// some unfortuanate constants
//  - these depend on the loaded network file for accuracy
#define SUIT_CLUB 0
#define SUIT_DIAMOND 1
#define SUIT_HEART 2
#define SUIT_SPADE 3
#define BORKED_I_MEAN_IT_IF_THIS_VALUE_IS_A_RESULT_SOMETHING_IS_SERIOUSLY_BROKEN_LIKE_REALLY_BROKE_LIKE_SO_BROKE_THIS_SHOULD_NEVER_HAPPEN___EVER_HELL_ITS_NOT_EVEN_USED 4

namespace ram {
	namespace vision {
        
        FANNSuitDetector::FANNSuitDetector (core::ConfigNode config, core::EventHubPtr eventHub):
        Detector (eventHub), m_imageDetector (ImageDetector (config, eventHub)), m_analyzed (0)
        {
        }
        
        void FANNSuitDetector::processImage (Image* input, Image* output)
        {
            if (output) {
                //(*output) = (*input);
            }
            m_analyzed = input->asIplImage();
            m_imageDetector.processImage (input);
        }
        
        IplImage* FANNSuitDetector::getAnalyzedImage ()
        {
            return m_analyzed;
        }
        
        Suit::SuitType FANNSuitDetector::getSuit ()
        {
            switch (m_imageDetector.getResult())
            {
                case SUIT_CLUB:
                    return Suit::CLUB;
                    break;
                case SUIT_DIAMOND:
                    return Suit::DIAMOND;
                    break;
                case SUIT_HEART:
                    return Suit::HEART;
                    break;
                case SUIT_SPADE:
                    return Suit::SPADE;
                    break;
                default:
                    return Suit::UNKNOWN;
                    break;
            };
        }
    }
}
