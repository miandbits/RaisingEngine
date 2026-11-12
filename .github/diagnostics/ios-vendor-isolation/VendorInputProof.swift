// Shared verbatim by S and E diagnostic harnesses; ordinary XCTest input is unchanged.
private struct VendorProofError:Error {let message:String}
func vendorReadRecords(_ path:String,_ device:String,_ inode:String) throws -> [[String:Any]] {
  let fd=open(path,O_RDONLY|O_NOFOLLOW|O_NONBLOCK)
  guard fd>=0 else {throw VendorProofError(message:"Cannot open bound stdout")}
  let file=FileHandle(fileDescriptor:fd,closeOnDealloc:true);defer{try? file.close()}
  var actual=stat(),current=stat()
  guard fstat(fd,&actual)==0,(actual.st_mode&S_IFMT)==S_IFREG,String(actual.st_dev)==device,String(actual.st_ino)==inode else {throw VendorProofError(message:"Bound stdout identity changed")}
  let bytes=try file.read(upToCount:8*1024*1024+1) ?? Data()
  guard bytes.count<=8*1024*1024,lstat(path,&current)==0,current.st_dev==actual.st_dev,current.st_ino==actual.st_ino,(current.st_mode&S_IFMT)==S_IFREG else {throw VendorProofError(message:"Bound stdout replaced or oversized")}
  return bytes.split(separator:0x0A,omittingEmptySubsequences:false).dropLast().compactMap {try? JSONSerialization.jsonObject(with:Data($0)) as? [String:Any]}
}
func validateVendorInputProof(_ proof:[String:Any],nonce:String,pid:pid_t,frame:CGRect,point:CGPoint,duration:Double?) throws {
  guard proof["version"] as? Int==1,proof["nonce"] as? String==nonce,proof["pid"] as? Int==Int(pid),proof["source"] as? String=="SDL_PollEvent",
    proof["overflow"] as? Bool==false,proof["terminal"] as? Bool==true,
    let samples=proof["samples"] as? [[String:Any]],samples.count>=2,samples.count<=64,
    samples.first?["phase"] as? String=="down",samples.last?["phase"] as? String=="up",
    samples.dropFirst().dropLast().allSatisfy({$0["phase"] as? String=="motion"}),
    let window=proof["windowID"] as? Int,window>0,
    let touch=samples[0]["touchID"] as? String,let touchValue=UInt64(touch),touchValue>0,
    let finger=samples[0]["fingerID"] as? String,let fingerValue=UInt64(finger),fingerValue>0 else {throw VendorProofError(message:"Missing, canceled or invalid actual SDL pair")}
  var previous:UInt64=0
  for sample in samples {
    guard sample["direct"] as? Bool==true,sample["mouseTouch"] as? Bool==false,sample["windowID"] as? Int==window,
      sample["touchID"] as? String==touch,sample["fingerID"] as? String==finger,sample["converted"] as? Bool==true,
      let timestamp=UInt64(sample["timestampNS"] as? String ?? ""),timestamp>=previous,
      let x=sample["x"] as? Double,let y=sample["y"] as? Double,let width=sample["windowWidth"] as? Double,let height=sample["windowHeight"] as? Double,
      x.isFinite,y.isFinite,abs(width-frame.width)<0.5,abs(height-frame.height)<0.5,
      abs(x*width-(point.x-frame.minX))<=0.5,abs(y*height-(point.y-frame.minY))<=0.5 else {throw VendorProofError(message:"Actual SDL identity/point/geometry differs")}
    previous=timestamp
  }
  let first=UInt64(samples[0]["timestampNS"] as? String ?? "")!,last=UInt64(samples.last?["timestampNS"] as? String ?? "")!
  guard last>first else {throw VendorProofError(message:"Actual SDL timestamps are not increasing")}
  if let duration {guard abs(Double(last-first)/1e9-duration)<=0.02 else {throw VendorProofError(message:"Actual SDL duration differs")}}
  guard let geometry=proof["geometryObservation"] as? [String:Any],geometry["overflow"] as? Bool==false,
    let count=geometry["eventCount"] as? Int,count>0,count<=512,
    let events=geometry["events"] as? [[String:Any]],events.count==count,
    (geometry["initial"] as? [String:Any])?["valid"] as? Bool==true,
    (geometry["final"] as? [String:Any])?["valid"] as? Bool==true else {throw VendorProofError(message:"Geometry observation incomplete")}
  // Successful input does not require geometry to be unchanged; retain changes
  // for causal comparison rather than turning the observer into a readiness gate.
}
