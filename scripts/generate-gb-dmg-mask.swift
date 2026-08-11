#!/usr/bin/env swift
import CoreGraphics
import Foundation
import ImageIO
import UniformTypeIdentifiers

let width = 1024
let height = 768
let viewport = CGRect(x: 112, y: 24, width: 800, height: 720)

let scriptURL = URL(fileURLWithPath: #filePath)
let projectRoot = scriptURL
    .deletingLastPathComponent()
    .deletingLastPathComponent()
    .deletingLastPathComponent()
let sourceURL = CommandLine.arguments.count > 1
    ? URL(fileURLWithPath: CommandLine.arguments[1])
    : projectRoot.appendingPathComponent("assets/generated/handheld-icons/gb-dmg-simple-source.png")
let outputURL = CommandLine.arguments.count > 2
    ? URL(fileURLWithPath: CommandLine.arguments[2])
    : projectRoot.appendingPathComponent("MinUI/skeleton/SYSTEM/res/gb-brick-mask-dmg-v2.png")

guard
    let sourceProvider = CGImageSourceCreateWithURL(sourceURL as CFURL, nil),
    let source = CGImageSourceCreateImageAtIndex(sourceProvider, 0, nil),
    let context = CGContext(
        data: nil,
        width: width,
        height: height,
        bitsPerComponent: 8,
        bytesPerRow: 0,
        space: CGColorSpaceCreateDeviceRGB(),
        bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue
    )
else {
    fatalError("Unable to load source image or create output context")
}

func color(_ red: CGFloat, _ green: CGFloat, _ blue: CGFloat, _ alpha: CGFloat = 1) -> CGColor {
    CGColor(red: red / 255, green: green / 255, blue: blue / 255, alpha: alpha)
}

func fill(_ rect: CGRect, _ fillColor: CGColor) {
    context.setFillColor(fillColor)
    context.fill(rect)
}

func roundedRect(_ rect: CGRect, radius: CGFloat, fillColor: CGColor, strokeColor: CGColor? = nil, lineWidth: CGFloat = 1) {
    let path = CGPath(roundedRect: rect, cornerWidth: radius, cornerHeight: radius, transform: nil)
    context.addPath(path)
    context.setFillColor(fillColor)
    context.fillPath()
    if let strokeColor {
        context.addPath(path)
        context.setStrokeColor(strokeColor)
        context.setLineWidth(lineWidth)
        context.strokePath()
    }
}

func drawCrop(_ cropRect: CGRect, into destination: CGRect) {
    guard let crop = source.cropping(to: cropRect) else { return }
    context.interpolationQuality = .high
    context.saveGState()
    context.translateBy(x: destination.minX, y: destination.maxY)
    context.scaleBy(x: 1, y: -1)
    context.draw(crop, in: CGRect(origin: .zero, size: destination.size))
    context.restoreGState()
}

context.translateBy(x: 0, y: CGFloat(height))
context.scaleBy(x: 1, y: -1)

let baseGradient = CGGradient(
    colorsSpace: CGColorSpaceCreateDeviceRGB(),
    colors: [color(239, 237, 230), color(215, 213, 205), color(229, 226, 218)] as CFArray,
    locations: [0, 0.58, 1]
)!
context.drawLinearGradient(
    baseGradient,
    start: CGPoint(x: 0, y: 0),
    end: CGPoint(x: CGFloat(width), y: CGFloat(height)),
    options: []
)

var randomState: UInt64 = 0x47424D41534B
for _ in 0..<12000 {
    randomState = randomState &* 6364136223846793005 &+ 1
    let x = Int((randomState >> 16) % UInt64(width))
    randomState = randomState &* 6364136223846793005 &+ 1
    let y = Int((randomState >> 16) % UInt64(height))
    let shade = CGFloat(Int((randomState >> 40) % 18) - 9)
    fill(CGRect(x: x, y: y, width: 1, height: 1), color(128 + shade, 128 + shade, 124 + shade, 0.07))
}

fill(CGRect(x: 0, y: 0, width: width, height: 5), color(250, 249, 244))
fill(CGRect(x: 0, y: height - 5, width: width, height: 5), color(127, 130, 126))
fill(CGRect(x: 0, y: 0, width: 5, height: height), color(246, 245, 239))
fill(CGRect(x: width - 5, y: 0, width: 5, height: height), color(105, 109, 106))

fill(CGRect(x: 104, y: 24, width: 8, height: 720), color(50, 51, 58))
fill(CGRect(x: 912, y: 24, width: 8, height: 720), color(50, 51, 58))
fill(CGRect(x: 112, y: 16, width: 800, height: 8), color(50, 51, 58))
fill(CGRect(x: 112, y: 744, width: 800, height: 8), color(50, 51, 58))
fill(CGRect(x: 106, y: 24, width: 2, height: 720), color(112, 113, 119))
fill(CGRect(x: 916, y: 24, width: 2, height: 720), color(245, 244, 237))

drawCrop(CGRect(x: 375, y: 182, width: 500, height: 31), into: CGRect(x: 132, y: 1, width: 760, height: 22))

roundedRect(
    CGRect(x: 9, y: 118, width: 94, height: 124),
    radius: 10,
    fillColor: color(79, 80, 88),
    strokeColor: color(42, 43, 48),
    lineWidth: 2
)
drawCrop(CGRect(x: 368, y: 307, width: 70, height: 82), into: CGRect(x: 14, y: 132, width: 84, height: 98))
context.setFillColor(color(54, 20, 25))
context.fillEllipse(in: CGRect(x: 35, y: 150, width: 19, height: 19))
context.setFillColor(color(190, 38, 56))
context.fillEllipse(in: CGRect(x: 39, y: 154, width: 11, height: 11))
context.setFillColor(color(255, 132, 145, 0.75))
context.fillEllipse(in: CGRect(x: 41, y: 155, width: 4, height: 3))

roundedRect(CGRect(x: 8, y: 492, width: 96, height: 102), radius: 12, fillColor: color(223, 220, 212, 0.72))
drawCrop(CGRect(x: 345, y: 720, width: 180, height: 180), into: CGRect(x: 13, y: 500, width: 86, height: 86))

roundedRect(CGRect(x: 916, y: 78, width: 104, height: 84), radius: 12, fillColor: color(224, 220, 212, 0.78))
drawCrop(CGRect(x: 695, y: 720, width: 220, height: 150), into: CGRect(x: 919, y: 86, width: 98, height: 67))

roundedRect(
    CGRect(x: 938, y: 225, width: 60, height: 320),
    radius: 12,
    fillColor: color(235, 232, 224, 0.82),
    strokeColor: color(173, 173, 169),
    lineWidth: 1
)
if let logo = source.cropping(to: CGRect(x: 348, y: 615, width: 365, height: 58)) {
    context.saveGState()
    context.translateBy(x: 968, y: 385)
    context.rotate(by: .pi / 2)
    context.translateBy(x: -150, y: 24)
    context.scaleBy(x: 1, y: -1)
    context.draw(logo, in: CGRect(x: 0, y: 0, width: 300, height: 48))
    context.restoreGState()
}

roundedRect(CGRect(x: 916, y: 640, width: 104, height: 100), radius: 12, fillColor: color(222, 219, 211, 0.8))
drawCrop(CGRect(x: 695, y: 935, width: 225, height: 190), into: CGRect(x: 920, y: 648, width: 96, height: 84))

context.clear(viewport)

guard
    let outputImage = context.makeImage(),
    let destination = CGImageDestinationCreateWithURL(outputURL as CFURL, UTType.png.identifier as CFString, 1, nil)
else {
    fatalError("Unable to create output image")
}
CGImageDestinationAddImage(destination, outputImage, nil)
guard CGImageDestinationFinalize(destination) else {
    fatalError("Unable to write output image")
}

print(outputURL.path)
