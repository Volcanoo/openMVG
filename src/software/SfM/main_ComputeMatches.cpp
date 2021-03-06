
// Copyright (c) 2012, 2013 Pierre MOULON.

// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "openMVG/sfm/sfm_data.hpp"
#include "openMVG/sfm/sfm_data_io.hpp"
#include "openMVG/sfm/pipelines/sfm_engine.hpp"
#include "openMVG/sfm/pipelines/sfm_features_provider.hpp"
#include "openMVG/sfm/pipelines/sfm_regions_provider.hpp"
#include "software/SfM/io_regions_type.hpp"

/// Generic Image Collection image matching
#include "openMVG/matching_image_collection/Matcher_Regions_AllInMemory.hpp"
#include "openMVG/matching_image_collection/GeometricFilter.hpp"
#include "openMVG/matching_image_collection/F_ACRobust.hpp"
#include "openMVG/matching_image_collection/E_ACRobust.hpp"
#include "openMVG/matching_image_collection/H_ACRobust.hpp"
#include "software/SfM/pairwiseAdjacencyDisplay.hpp"
#include "openMVG/matching/indMatch_utils.hpp"
#include "openMVG/system/timer.hpp"

#include "third_party/cmdLine/cmdLine.h"
#include "third_party/stlplus3/filesystemSimplified/file_system.hpp"

#include <cstdlib>
#include <fstream>

using namespace openMVG;
using namespace openMVG::matching;
using namespace openMVG::robust;
using namespace std;

enum EGeometricModel
{
  FUNDAMENTAL_MATRIX = 0,
  ESSENTIAL_MATRIX   = 1,
  HOMOGRAPHY_MATRIX  = 2
};

enum EPairMode
{
  PAIR_EXHAUSTIVE = 0,
  PAIR_CONTIGUOUS = 1,
  PAIR_FROM_FILE  = 2
};

/// Compute corresponding features between a series of views:
/// - Load view images description (regions: features & descriptors)
/// - Compute putative local feature matches (descriptors matching)
/// - Compute geometric coherent feature matches (robust model estimation from putative matches)
/// - Export computed data
int main(int argc, char **argv)
{
  CmdLine cmd;

  std::string sSfM_Data_Filename;
  std::string sMatchesDirectory = "";
  std::string sGeometricModel = "f";
  float fDistRatio = .6f;
  int iMatchingVideoMode = -1;
  std::string sPredefinedPairList = "";
  bool bUpRight = false;
  std::string sNearestMatchingMethod = "AUTO";
  bool bForce = false;

  //required
  cmd.add( make_option('i', sSfM_Data_Filename, "input_file") );
  cmd.add( make_option('o', sMatchesDirectory, "out_dir") );
  // Options
  cmd.add( make_option('r', fDistRatio, "ratio") );
  cmd.add( make_option('g', sGeometricModel, "geometricModel") );
  cmd.add( make_option('v', iMatchingVideoMode, "videoModeMatching") );
  cmd.add( make_option('l', sPredefinedPairList, "pairList") );
  cmd.add( make_option('n', sNearestMatchingMethod, "nearestMatchingMethod") );
  cmd.add( make_option('f', bForce, "force") );

  try {
      if (argc == 1) throw std::string("Invalid command line parameter.");
      cmd.process(argc, argv);
  } catch(const std::string& s) {
      std::cerr << "Usage: " << argv[0] << '\n'
      << "[-i|--input_file]: a SfM_Data file\n"
      << "[-o|--out_dir path]\n"
      << "\n[Optional]\n"
      << "[-f|--force: Force to recompute data]\n"
      << "[-r|--ratio Distance ratio to discard non meaningful matches\n"
      << "   0.6 typical value (you can use 0.8 to have more matches)]\n"
      << "[-g|--geometricModel\n"
      << "  (pairwise correspondences filtering thanks to robust model estimation):\n"
      << "   f: fundamental matrix,\n"
      << "   e: essential matrix,\n"
      << "   h: homography matrix]\n"
      << "[-v|--videoModeMatching\n"
      << "  (sequence matching with an overlap of X images)\n"
      << "   X: with match 0 with (1->X), ...]\n"
      << "   2: will match 0 with (1,2), 1 with (2,3), ...\n"
      << "   3: will match 0 with (1,2,3), 1 with (2,3,4), ...]\n"
      << "[-l]--pairList file\n"
      << "[-n|--nearestMatchingMethod\n"
      << "  AUTO: auto choice from regions type,\n"
      << "  BRUTEFORCEL2: BruteForce L2 matching for Scalar based regions descriptor,\n"
      << "  BRUTEFORCEHAMMING: BruteForce Hamming matching for binary based regions descriptor,\n"
      << "  ANNL2: Approximate Nearest Neighbor L2 matching for Scalar based regions descriptor.]\n"
      << std::endl;

      std::cerr << s << std::endl;
      return EXIT_FAILURE;
  }

  std::cout << " You called : " <<std::endl
            << argv[0] << std::endl
            << "--input_file " << sSfM_Data_Filename << std::endl
            << "--outdir " << sMatchesDirectory << std::endl
            << "--ratio " << fDistRatio << std::endl
            << "--geometricModel " << sGeometricModel << std::endl
            << "--videoModeMatching " << iMatchingVideoMode << std::endl
            << "--nearestMatchingMethod " << sNearestMatchingMethod << std::endl;

  EPairMode ePairmode = (iMatchingVideoMode == -1 ) ? PAIR_EXHAUSTIVE : PAIR_CONTIGUOUS;

  if (sPredefinedPairList.length()) {
    std::cout << "--pairList " << sPredefinedPairList << std::endl;
    ePairmode = PAIR_FROM_FILE;
    if (iMatchingVideoMode>0) {
      std::cerr << "\nIncompatible options: --videoModeMatching and --pairList" << std::endl;
      return EXIT_FAILURE;
    }
  }

  if (sMatchesDirectory.empty())  {
    std::cerr << "\nIt is an invalid output directory" << std::endl;
    return EXIT_FAILURE;
  }

  EGeometricModel eGeometricModelToCompute = FUNDAMENTAL_MATRIX;
  std::string sGeometricMatchesFilename = "";
  switch(sGeometricModel[0])
  {
    case 'f': case 'F':
      eGeometricModelToCompute = FUNDAMENTAL_MATRIX;
      sGeometricMatchesFilename = "matches.f.txt";
    break;
    case 'e': case 'E':
      eGeometricModelToCompute = ESSENTIAL_MATRIX;
      sGeometricMatchesFilename = "matches.e.txt";
    break;
    case 'h': case 'H':
      eGeometricModelToCompute = HOMOGRAPHY_MATRIX;
      sGeometricMatchesFilename = "matches.h.txt";
    break;
    default:
      std::cerr << "Unknown geometric model" << std::endl;
      return EXIT_FAILURE;
  }

  // -----------------------------
  // - Load SfM_Data Views & intrinsics data
  // a. Compute putative descriptor matches
  // b. Geometric filtering of putative matches
  // + Export some statistics
  // -----------------------------

  //---------------------------------------
  // Read SfM Scene (image view & intrinsics data)
  //---------------------------------------
  SfM_Data sfm_data;
  if (!Load(sfm_data, sSfM_Data_Filename, ESfM_Data(VIEWS|INTRINSICS))) {
    std::cerr << std::endl
      << "The input SfM_Data file \""<< sSfM_Data_Filename << "\" cannot be read." << std::endl;
    return EXIT_FAILURE;
  }

  //---------------------------------------
  // Load SfM Scene regions
  //---------------------------------------
  // Init the regions_type from the image describer file (used for image regions extraction)
  using namespace openMVG::features;
  const std::string sImage_describer = stlplus::create_filespec(sMatchesDirectory, "image_describer", "json");
  std::unique_ptr<Regions> regions_type = Init_region_type_from_file(sImage_describer);
  if (!regions_type)
  {
    std::cerr << "Invalid: "
      << sImage_describer << " regions type file." << std::endl;
    return EXIT_FAILURE;
  }

  //---------------------------------------
  // a. Compute putative descriptor matches
  //    - Descriptor matching (according user method choice)
  //    - Keep correspondences only if NearestNeighbor ratio is ok
  //---------------------------------------
  PairWiseMatches map_PutativesMatches;

  // Build some alias from SfM_Data Views data:
  // - List views as a vector of filenames & image sizes
  std::vector<std::string> vec_fileNames;
  std::vector<std::pair<size_t, size_t> > vec_imagesSize;
  {
    vec_fileNames.reserve(sfm_data.getViews().size());
    vec_imagesSize.reserve(sfm_data.getViews().size());
    for (Views::const_iterator iter = sfm_data.getViews().begin();
      iter != sfm_data.getViews().end();
      ++iter)
    {
      const View * v = iter->second.get();
      vec_fileNames.push_back(stlplus::create_filespec(sfm_data.s_root_path,
          v->s_Img_path));
      vec_imagesSize.push_back( std::make_pair( v->ui_width, v->ui_height) );
    }
  }

  std::cout << std::endl << " - PUTATIVE MATCHES - " << std::endl;
  // If the matches already exists, reload them
  if (!bForce && stlplus::file_exists(sMatchesDirectory + "/matches.putative.txt"))
  {
    PairedIndMatchImport(sMatchesDirectory + "/matches.putative.txt", map_PutativesMatches);
    std::cout << "\t PREVIOUS RESULTS LOADED" << std::endl;
  }
  else // Compute the putative matches
  {
    std::cout << "Use: ";
    switch (ePairmode)
    {
      case PAIR_EXHAUSTIVE: std::cout << "exhaustive pairwise matching" << std::endl; break;
      case PAIR_CONTIGUOUS: std::cout << "sequence pairwise matching" << std::endl; break;
      case PAIR_FROM_FILE:  std::cout << "user defined pairwise matching" << std::endl; break;
    }

    // Allocate the right Matcher according the Matching requested method
    std::unique_ptr<Matcher_Regions_AllInMemory> collectionMatcher;
    if (sNearestMatchingMethod == "AUTO")
    {
      if (regions_type->IsScalar())
      {
        collectionMatcher.reset(new Matcher_Regions_AllInMemory(fDistRatio, ANN_L2));
      }
      else
      if (regions_type->IsBinary())
      {
        collectionMatcher.reset(new Matcher_Regions_AllInMemory(fDistRatio, BRUTE_FORCE_HAMMING));
      }
    }
    else
    if (sNearestMatchingMethod == "BRUTEFORCEL2")
    {
      collectionMatcher.reset(new Matcher_Regions_AllInMemory(fDistRatio, BRUTE_FORCE_L2));
    }
    else
    if (sNearestMatchingMethod == "BRUTEFORCEHAMMING")
    {
      collectionMatcher.reset(new Matcher_Regions_AllInMemory(fDistRatio, BRUTE_FORCE_HAMMING));
    }
    else
    if (sNearestMatchingMethod == "ANNL2")
    {
      collectionMatcher.reset(new Matcher_Regions_AllInMemory(fDistRatio, ANN_L2));
    }
    if (!collectionMatcher)
    {
      std::cerr << "Invalid Nearest Neighbor method: " << sNearestMatchingMethod << std::endl;
      return EXIT_FAILURE;
    }
    // Perform the matching
    Timer timer;
    if (collectionMatcher->loadData(regions_type, vec_fileNames, sMatchesDirectory))
    {
      // From matching mode compute the pair list that have to be matched:
      Pair_Set pairs;
      switch (ePairmode)
      {
        case PAIR_EXHAUSTIVE: pairs = exhaustivePairs(sfm_data.getViews().size()); break;
        case PAIR_CONTIGUOUS: pairs = contiguousWithOverlap(sfm_data.getViews().size(), iMatchingVideoMode); break;
        case PAIR_FROM_FILE:
          if(!loadPairs(sfm_data.getViews().size(), sPredefinedPairList, pairs))
          {
              return EXIT_FAILURE;
          };
          break;
      }
      // Photometric matching of putative pairs
      collectionMatcher->Match(vec_fileNames, pairs, map_PutativesMatches);
      //---------------------------------------
      //-- Export putative matches
      //---------------------------------------
      std::ofstream file (std::string(sMatchesDirectory + "/matches.putative.txt").c_str());
      if (file.is_open())
        PairedIndMatchToStream(map_PutativesMatches, file);
      file.close();
    }
    std::cout << "Task (Regions Loading+Matching) done in (s): " << timer.elapsed() << std::endl;
  }
  //-- export putative matches Adjacency matrix
  PairWiseMatchingToAdjacencyMatrixSVG(vec_fileNames.size(),
    map_PutativesMatches,
    stlplus::create_filespec(sMatchesDirectory, "PutativeAdjacencyMatrix", "svg"));

  //---------------------------------------
  // b. Geometric filtering of putative matches
  //    - AContrario Estimation of the desired geometric model
  //    - Use an upper bound for the a contrario estimated threshold
  //---------------------------------------

  // Load the features
  std::shared_ptr<Features_Provider> feats_provider = std::make_shared<Features_Provider>();
  if (!feats_provider->load(sfm_data, sMatchesDirectory, regions_type)) {
    std::cerr << std::endl << "Invalid features." << std::endl;
    return EXIT_FAILURE;
  }

  PairWiseMatches map_GeometricMatches;

  ImageCollectionGeometricFilter collectionGeomFilter(feats_provider.get());
  const double maxResidualError = 4.0;
  {
    Timer timer;
    std::cout << std::endl << " - GEOMETRIC FILTERING - " << std::endl;
    switch (eGeometricModelToCompute)
    {
      case FUNDAMENTAL_MATRIX:
      {
       collectionGeomFilter.Filter(
          GeometricFilter_FMatrix_AC(maxResidualError),
          map_PutativesMatches,
          map_GeometricMatches,
          vec_imagesSize);
      }
      break;
      case ESSENTIAL_MATRIX:
      {
        // Build the intrinsic parameter map for each view
        std::map<IndexT, Mat3> map_K;
        size_t cpt = 0;
        for (Views::const_iterator iter = sfm_data.getViews().begin();
          iter != sfm_data.getViews().end();
          ++iter, ++cpt)
        {
          const View * v = iter->second.get();
          if (sfm_data.getIntrinsics().count(v->id_intrinsic))
          {
            const IntrinsicBase * ptrIntrinsic = sfm_data.getIntrinsics().find(v->id_intrinsic)->second.get();
            switch (ptrIntrinsic->getType())
            {
              case PINHOLE_CAMERA:
              case PINHOLE_CAMERA_RADIAL1:
              case PINHOLE_CAMERA_RADIAL3:
                const Pinhole_Intrinsic * ptrPinhole = (const Pinhole_Intrinsic*)(ptrIntrinsic);
                map_K[cpt] = ptrPinhole->K();
              break;
            }
          }
        }

        collectionGeomFilter.Filter(
          GeometricFilter_EMatrix_AC(map_K, maxResidualError),
          map_PutativesMatches,
          map_GeometricMatches,
          vec_imagesSize);

        //-- Perform an additional check to remove pairs with poor overlap
        std::vector<PairWiseMatches::key_type> vec_toRemove;
        for (PairWiseMatches::const_iterator iterMap = map_GeometricMatches.begin();
          iterMap != map_GeometricMatches.end(); ++iterMap)
        {
          const size_t putativePhotometricCount = map_PutativesMatches.find(iterMap->first)->second.size();
          const size_t putativeGeometricCount = iterMap->second.size();
          const float ratio = putativeGeometricCount / (float)putativePhotometricCount;
          if (putativeGeometricCount < 50 || ratio < .3f)  {
            // the pair will be removed
            vec_toRemove.push_back(iterMap->first);
          }
        }
        //-- remove discarded pairs
        for (std::vector<PairWiseMatches::key_type>::const_iterator
          iter =  vec_toRemove.begin(); iter != vec_toRemove.end(); ++iter)
        {
          map_GeometricMatches.erase(*iter);
        }
      }
      break;
      case HOMOGRAPHY_MATRIX:
      {
        collectionGeomFilter.Filter(
          GeometricFilter_HMatrix_AC(maxResidualError),
          map_PutativesMatches,
          map_GeometricMatches,
          vec_imagesSize);
      }
      break;
    }

    //---------------------------------------
    //-- Export geometric filtered matches
    //---------------------------------------
    std::ofstream file (string(sMatchesDirectory + "/" + sGeometricMatchesFilename).c_str());
    if (file.is_open())
      PairedIndMatchToStream(map_GeometricMatches, file);
    file.close();

    std::cout << "Task done in (s): " << timer.elapsed() << std::endl;

    //-- export Adjacency matrix
    std::cout << "\n Export Adjacency Matrix of the pairwise's geometric matches"
      << std::endl;
    PairWiseMatchingToAdjacencyMatrixSVG(vec_fileNames.size(),
      map_GeometricMatches,
      stlplus::create_filespec(sMatchesDirectory, "GeometricAdjacencyMatrix", "svg"));
  }
  return EXIT_SUCCESS;
}
