/*
* Copyright 2010-2015 Amazon.com, Inc. or its affiliates. All Rights Reserved.
*
* Licensed under the Apache License, Version 2.0 (the "License").
* You may not use this file except in compliance with the License.
* A copy of the License is located at
*
*  http://aws.amazon.com/apache2.0
*
* or in the "license" file accompanying this file. This file is distributed
* on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
* express or implied. See the License for the specific language governing
* permissions and limitations under the License.
*/
#include <aws/s3/model/ReplicationConfiguration.h>
#include <aws/core/utils/xml/XmlSerializer.h>
#include <aws/core/utils/StringUtils.h>
#include <aws/core/utils/memory/stl/AWSStringStream.h>

#include <utility>

using namespace Aws::S3::Model;
using namespace Aws::Utils::Xml;
using namespace Aws::Utils;

ReplicationConfiguration::ReplicationConfiguration()
{
}

ReplicationConfiguration::ReplicationConfiguration(const XmlNode& xmlNode)
{
  *this = xmlNode;
}

ReplicationConfiguration& ReplicationConfiguration::operator =(const XmlNode& xmlNode)
{
  XmlNode resultNode = xmlNode;

  if(!resultNode.IsNull())
  {
    XmlNode roleNode = resultNode.FirstChild("Role");
    m_role = StringUtils::Trim(roleNode.GetText().c_str());
    XmlNode rulesNode = resultNode.FirstChild("Rules");
    while(!rulesNode.IsNull())
    {
      m_rules.push_back(rulesNode);
      rulesNode = rulesNode.NextNode("Rules");
    }

  }

  return *this;
}

void ReplicationConfiguration::AddToNode(XmlNode& parentNode) const
{
  Aws::StringStream ss;
  XmlNode roleNode = parentNode.CreateChildElement("Rule");
  roleNode.SetText(m_role);
  for(const auto& item : m_rules)
  {
    XmlNode rulesNode = parentNode.CreateChildElement("Rule");
    item.AddToNode(rulesNode);
  }
}